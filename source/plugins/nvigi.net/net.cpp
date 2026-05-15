// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.extra/extra.h"
#include "source/plugins/nvigi.net/net.h"
#define CURL_STATICLIB
#include "external/libcurl/include/curl/curl.h"
#include "external/json/source/nlohmann/json.hpp"
#include <mutex>

using json = nlohmann::json;

namespace nvigi
{
namespace net
{

template <typename T>
static T getJSONValue(const json& body, const std::string& key, const T& default_value)
{
    // Fallback null to default value
    return body.contains(key) && !body.at(key).is_null() ? body.value(key, default_value) : default_value;
}

// Response buffer with size limit enforcement
struct ResponseBuffer {
    std::string* data;
    uint64_t maxSize;
    uint64_t currentSize = 0;
};

static size_t curlCallbackWriteStdString(void* contents, size_t size, size_t nmemb, ResponseBuffer* buffer)
{
    size_t newLength = size * nmemb;
    
    // Enforce response size limit if configured (0 = unlimited)
    if (buffer->maxSize > 0 && buffer->currentSize + newLength > buffer->maxSize)
    {
        NVIGI_LOG_ERROR("Response exceeds maximum size limit of %llu bytes (current: %llu, new: %zu)", 
                        buffer->maxSize, buffer->currentSize, newLength);
        return 0; // Abort transfer
    }
    
    try
    {
        buffer->data->append((char*)contents, newLength);
        buffer->currentSize += newLength;
    }
    catch (std::bad_alloc& e)
    {
        NVIGI_LOG_ERROR("bad_alloc exception in CURL write callback - %s", e.what());
        return 0;
    }
    return newLength;
};

struct MemoryStruct {
    const uint8_t* memory;
    size_t size;
    size_t position;
};

size_t curlCallbackReadBytes(void* ptr, size_t size, size_t nmemb, void* userp)
{
    MemoryStruct* mem = (MemoryStruct*)userp;

    if (size * nmemb < 1)
        return 0;

    if (mem->position < mem->size) {
        size_t buffer_size = size * nmemb;
        size_t remaining_data = mem->size - mem->position;
        size_t copy_this_much = buffer_size < remaining_data ? buffer_size : remaining_data;

        memcpy(ptr, mem->memory + mem->position, copy_this_much);
        mem->position += copy_this_much;

        return copy_this_much;
    }

    return 0; // No more data left to deliver
}

// Custom debug function with optional credential redaction
int curlCallbackDebug(CURL* handle, curl_infotype type, char* data, size_t size, void* userptr) 
{
    // Note: This gets called ONLY if CURLOPT_VERBOSE is set to 1 which is controlled via runtime parameters
    bool* redactAuth = static_cast<bool*>(userptr);

    if (type == CURLINFO_TEXT) {
        std::string text(data, size);
        NVIGI_LOG("curl", LogType::eInfo, log::ConsoleForeground::WHITE, "%s", text.c_str());
    }
    else if (type == CURLINFO_HEADER_IN || type == CURLINFO_HEADER_OUT) {
        std::string headerData(data, size);
        
        // Redact Authorization header if requested
        if (redactAuth && *redactAuth) {
            size_t authPos = headerData.find("Authorization:");
            if (authPos != std::string::npos) {
                size_t lineEnd = headerData.find("\r\n", authPos);
                if (lineEnd != std::string::npos) {
                    headerData = headerData.substr(0, authPos) + "Authorization: [REDACTED]\r\n" + 
                                 headerData.substr(lineEnd + 2);
                } else {
                    headerData = headerData.substr(0, authPos) + "Authorization: [REDACTED]";
                }
            }
        }
        
        const char* logPrefix = (type == CURLINFO_HEADER_IN) ? "curl][header-in" : "curl][header-out";
        NVIGI_LOG(logPrefix, LogType::eInfo, log::ConsoleForeground::WHITE, "%s", headerData.c_str());
    }
    else if (type == CURLINFO_DATA_IN) {
        std::string dataIn(data, size);
        NVIGI_LOG("curl][data-in", LogType::eInfo, log::ConsoleForeground::WHITE, "%s", dataIn.c_str());
    }
    else if (type == CURLINFO_DATA_OUT) {
        std::string dataOut(data, size);
        NVIGI_LOG("curl][data-out", LogType::eInfo, log::ConsoleForeground::WHITE, "%s", dataOut.c_str());
    }
    return 0; // Return 0 to indicate success
}

// Structure to pass callback, userdata, and limits to CURL for streaming
struct StreamingCallbackData {
    StreamingDataCallback callback;
    void* userdata;
    uint64_t maxSize;
    uint64_t bytesReceived = 0;
};

// Create a custom write callback for streaming data with size limit enforcement
size_t streamingWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* cbData = static_cast<StreamingCallbackData*>(userp);
    size_t realsize = size * nmemb;
    
    // Enforce streaming size limit if configured (0 = unlimited)
    if (cbData->maxSize > 0 && cbData->bytesReceived + realsize > cbData->maxSize) {
        NVIGI_LOG_ERROR("Streaming response exceeds maximum size limit of %llu bytes (current: %llu, new: %zu)", 
                        cbData->maxSize, cbData->bytesReceived, realsize);
        return 0; // Abort transfer
    }
    
    cbData->bytesReceived += realsize;
    return cbData->callback(static_cast<const char*>(contents), realsize, cbData->userdata);
};

struct Network : public INetworkInternal
{
    // Helper function to apply security settings from Parameters to CURL handle
    void applySecuritySettings(CURL* handle, const Parameters& params)
    {
        // SSL/TLS Verification
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, params.verifySSLPeer ? 1L : 0L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, params.verifySSLHost ? 2L : 0L);
        
        if (!params.sslCABundlePath.empty())
        {
            curl_easy_setopt(handle, CURLOPT_CAINFO, params.sslCABundlePath.c_str());
        }
        
        // Timeout settings
        if (params.timeoutSeconds > 0)
        {
            curl_easy_setopt(handle, CURLOPT_TIMEOUT, static_cast<long>(params.timeoutSeconds));
        }
        
        if (params.connectionTimeoutSeconds > 0)
        {
            curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, static_cast<long>(params.connectionTimeoutSeconds));
        }
        
        if (params.lowSpeedLimit > 0 && params.lowSpeedTimeSeconds > 0)
        {
            curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, static_cast<long>(params.lowSpeedLimit));
            curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, static_cast<long>(params.lowSpeedTimeSeconds));
        }
        
        // Protocol restrictions
        long allowedProtocols = params.allowHTTP ? (CURLPROTO_HTTP | CURLPROTO_HTTPS) : CURLPROTO_HTTPS;
        curl_easy_setopt(handle, CURLOPT_PROTOCOLS, allowedProtocols);
        curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS, allowedProtocols);
        
        // Redirect settings
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, params.followRedirects ? 1L : 0L);
        if (params.followRedirects)
        {
            curl_easy_setopt(handle, CURLOPT_MAXREDIRS, static_cast<long>(params.maxRedirects));
        }
        
        // Prevent path traversal in URLs
        curl_easy_setopt(handle, CURLOPT_PATH_AS_IS, 0L);
        
        // Store redaction flag and setup debug callback
        currentRedactAuthFlag = params.redactAuthInLogs;
        curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, curlCallbackDebug);
        curl_easy_setopt(handle, CURLOPT_DEBUGDATA, &currentRedactAuthFlag);
    }

    virtual Result initialize() override final
    {
        // Initialize CURL globally (required for thread safety and SSL)
        CURLcode globalInit = curl_global_init(CURL_GLOBAL_ALL);
        if (globalInit != CURLE_OK)
        {
            NVIGI_LOG_ERROR("Unable to initialize CURL globally - %s", curl_easy_strerror(globalInit));
            return kResultNetFailedToInitializeCurl;
        }
        
        curl = curl_easy_init();
        if (!curl)
        {
            NVIGI_LOG_ERROR("Unable to initialize CURL library");
            curl_global_cleanup();
            return kResultNetFailedToInitializeCurl;
        }
        
        NVIGI_LOG_VERBOSE("CURL initialized successfully");
        return kResultOk;
    }

    virtual Result shutdown() override final
    {
        if (curl)
        {
            curl_easy_cleanup(curl);
            curl = {};
        }
        
        // Cleanup CURL globally
        curl_global_cleanup();
        
        NVIGI_LOG_VERBOSE("CURL shut down successfully");
        return kResultOk;
    }

    virtual Result setVerboseMode(bool flag) override final
    {
        verboseMode = flag;
        return kResultOk;
    }

    virtual Result setAuthToken(const char* token) override final
    {
        gfnKey = token;
        return kResultOk;
    }

    std::string resolveAuthToken(const Parameters& params) const
    {
        return !params.authToken.empty() ? std::string(params.authToken.c_str()) : gfnKey;
    }

    virtual Result httpGet(const Parameters& params, std::string& response) override final
    {
        // Apply security settings first
        applySecuritySettings(curl, params);
        
        curl_easy_setopt(curl, CURLOPT_URL, params.url.c_str());

        struct curl_slist* headers = NULL;
        for (auto h : params.headers)
        {
            headers = curl_slist_append(headers, h.c_str());
        }

        auto bearerToken = resolveAuthToken(params);
        if (!bearerToken.empty())
        {
            headers = curl_slist_append(headers, ("Authorization: Bearer " + bearerToken).c_str());
        }

        std::string tmp;
        ResponseBuffer buffer{ &tmp, params.maxResponseSizeBytes, 0 };
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallbackWriteStdString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, verboseMode ? 1 : 0);
        
        NVIGI_LOG_VERBOSE("Connecting to '%s', awaiting response ...", params.url.c_str());
        auto res = curl_easy_perform(curl);

        if (res != CURLE_OK)
        {
            NVIGI_LOG_ERROR("CURL GET request failed with error - %s", curl_easy_strerror(res));
            curl_easy_reset(curl);
            curl_slist_free_all(headers);
            return kResultNetCurlError;
        }

        curl_easy_reset(curl);
        curl_slist_free_all(headers);

        NVIGI_LOG_VERBOSE("CURL GET request returned %llu bytes", buffer.currentSize);

        response = std::move(tmp);
        return kResultOk;
    }

    virtual Result httpPost(const Parameters& params, std::string& response) override final
    {
        // Apply security settings first
        applySecuritySettings(curl, params);
        
        curl_easy_setopt(curl, CURLOPT_URL, params.url.c_str());

        struct curl_slist* headers = NULL;
        for (auto h : params.headers)
        {
            headers = curl_slist_append(headers, h.c_str());
        }
        
        auto bearerToken = resolveAuthToken(params);
        if (!bearerToken.empty())
        {
            headers = curl_slist_append(headers, ("Authorization: Bearer " + bearerToken).c_str());
        }

        std::string tmp;
        ResponseBuffer buffer{ &tmp, params.maxResponseSizeBytes, 0 };
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.data.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, params.data.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallbackWriteStdString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, verboseMode ? 1 : 0);

        NVIGI_LOG_VERBOSE("Connecting to '%s', awaiting response ...", params.url.c_str());
        auto res = curl_easy_perform(curl);
        
        if (res != CURLE_OK)
        {
            NVIGI_LOG_ERROR("CURL POST request returned %llu bytes - %s", buffer.currentSize, tmp.c_str());
            NVIGI_LOG_ERROR("CURL POST request failed with error - %s", curl_easy_strerror(res));
            curl_easy_reset(curl);
            curl_slist_free_all(headers);
            return kResultNetCurlError;
        }

        curl_easy_reset(curl);
        curl_slist_free_all(headers);

        // If status polling is enabled, parse as JSON and poll for completion (NVCF-style)
        if (params.enableStatusPolling)
        {
            try
            {
                json jsonResponse = json::parse(tmp);

                constexpr float kMaxWaitMs = 5000.0f;
                constexpr const char* kStatusOK = "fulfilled";

                auto getStatus = [](json& resp)->std::string
                {
                    std::string status = "fulfilled";
                    if (resp.contains("status"))
                    {
                        if (resp.at("status").is_string())
                        {
                            status = resp.value("status", status);
                        }
                        else if (resp.at("status").is_number_integer())
                        {
                            status = std::to_string(resp.at("status").operator int());
                        }
                    }
                    else if (resp.contains("error"))
                    {
                        if (resp.at("error").is_string())
                        {
                            status = resp.value("error", status);
                        }
                        else if (resp.at("error").is_number_integer())
                        {
                            status = std::to_string(resp.at("error").operator int());
                        }
                    }
                    return status;
                };

                auto status = getStatus(jsonResponse);
                auto tStart = std::chrono::high_resolution_clock::now();

                // Determine the base URL for status polling
                std::string pollBaseUrl = !params.statusPollingBaseUrl.empty()
                    ? std::string(params.statusPollingBaseUrl.c_str())
                    : "https://api.nvcf.nvidia.com/v2/nvcf/exec/status/";
                // Ensure trailing slash
                if (!pollBaseUrl.empty() && pollBaseUrl.back() != '/') pollBaseUrl += '/';

                while (status == "pending-evaluation")
                {
                    std::string reqId = jsonResponse["reqId"];
                    // Inherit caller's params (auth, SSL, timeouts) for the status poll
                    Parameters statusParams = params;
                    statusParams.url = (pollBaseUrl + reqId).c_str();
                    statusParams.enableStatusPolling = false; // Prevent recursive polling
                    statusParams.data = {};  // GET request, no body
                    std::string statusRawResponse;
                    if (httpGet(statusParams, statusRawResponse) != kResultOk) break;
                    jsonResponse = json::parse(statusRawResponse);
                    status = getStatus(jsonResponse);
                    std::chrono::duration<float, std::milli> tElapsed = std::chrono::high_resolution_clock::now() - tStart;
                    if (tElapsed.count() > kMaxWaitMs)
                    {
                        status = extra::format("timed out after {}ms", kMaxWaitMs);
                        break;
                    }
                }
                if (status != kStatusOK)
                {
                    NVIGI_LOG_WARN("POST request failed, status '%s', details: '%s'", status.c_str(), tmp.c_str());
                    response = std::move(tmp);
                    return kResultNetServerError;
                }
                response = jsonResponse.dump(-1, ' ', false, json::error_handler_t::replace);
            }
            catch (std::exception&)
            {
                NVIGI_LOG_WARN("Status polling enabled but server returned non-JSON response '%s'", tmp.c_str());
                response = std::move(tmp);
                return kResultNetServerError;
            }
        }
        else
        {
            response = std::move(tmp);
        }
        return kResultOk;
    }

    virtual Result httpPostStreaming(const Parameters& params, StreamingDataCallback callback, void* userdata) override final
    {
        // Apply security settings first
        applySecuritySettings(curl, params);
        
        curl_easy_setopt(curl, CURLOPT_URL, params.url.c_str());

        struct curl_slist* headers = NULL;
        for (auto h : params.headers)
        {
            headers = curl_slist_append(headers, h.c_str());
        }
        
        auto bearerToken = resolveAuthToken(params);
        if (!bearerToken.empty())
        {
            headers = curl_slist_append(headers, ("Authorization: Bearer " + bearerToken).c_str());
        }

        StreamingCallbackData callbackData = { callback, userdata, params.maxStreamingSizeBytes, 0 };

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.data.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, params.data.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streamingWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &callbackData);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, verboseMode ? 1 : 0);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 128L); // Small buffer for frequent callbacks

        NVIGI_LOG_VERBOSE("Connecting to '%s' for streaming response ...", params.url.c_str());
        auto res = curl_easy_perform(curl);
        
        if (res != CURLE_OK)
        {
            NVIGI_LOG_ERROR("CURL streaming POST request failed with error - %s", curl_easy_strerror(res));
            curl_easy_reset(curl);
            curl_slist_free_all(headers);
            return kResultNetCurlError;
        }

        NVIGI_LOG_VERBOSE("Streaming completed, received %llu bytes", callbackData.bytesReceived);
        curl_easy_reset(curl);
        curl_slist_free_all(headers);
        return kResultOk;
    }

    virtual Result uploadAsset(const types::string& contentType, const types::string& description, const types::vector<uint8_t>& asset, types::string& assetId) override final
    {
        // First we need to obtain asset id and URL from NVCF
        net::Parameters params;
        params.url = "https://api.nvcf.nvidia.com/v2/nvcf/assets";
        params.headers = { "Content-Type: application/json" };
        params.enableStatusPolling = true;

        json data;
        data["contentType"] = contentType.c_str();
        data["description"] = description.c_str();

        std::string jsonObj = data.dump(-1, ' ', false, json::error_handler_t::replace);

        params.data.resize(jsonObj.size());
        memcpy(params.data.data(), jsonObj.c_str(), jsonObj.size());
        
        std::string responseStr;
        auto res = httpPost(params, responseStr);
        if (res == kResultOk)
        {
            try
            {
                json response = json::parse(responseStr);

                // We got our id and url, now we can upload the data
                assetId = response["assetId"].operator std::string().c_str();
                std::string uploadUrl = response["uploadUrl"];

                NVIGI_LOG_VERBOSE("Uploaded asset id '%s'", assetId.c_str());

                MemoryStruct uploadData = { asset.data(), asset.size(), 0};

                // Create upload parameters with secure defaults for the PUT request
                Parameters uploadParams;
                uploadParams.url = uploadUrl.c_str();
                // Apply security settings for the upload
                applySecuritySettings(curl, uploadParams);

                struct curl_slist* headers = NULL;
                headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
                headers = curl_slist_append(headers, ("x-amz-meta-nvcf-asset-description: " + description).c_str());
            
                std::string tmp;
                ResponseBuffer buffer{ &tmp, uploadParams.maxResponseSizeBytes, 0 };
            
                curl_easy_setopt(curl, CURLOPT_URL, uploadUrl.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_PUT, 1L);
                curl_easy_setopt(curl, CURLOPT_READFUNCTION, curlCallbackReadBytes);
                curl_easy_setopt(curl, CURLOPT_READDATA, (void*)&uploadData);
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
                curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)uploadData.size);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallbackWriteStdString);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
                curl_easy_setopt(curl, CURLOPT_VERBOSE, verboseMode ? 1 : 0);

                auto res = curl_easy_perform(curl);

                curl_easy_reset(curl);
                curl_slist_free_all(headers);

                NVIGI_LOG_VERBOSE("CURL upload request returned %llu bytes - %s", buffer.currentSize, tmp.c_str());

                if (res != CURLE_OK)
                {
                    NVIGI_LOG_ERROR("CURL request failed with error - %s", curl_easy_strerror(res));
                    return kResultNetCurlError;
                }
            }
            catch (std::exception& e)
            {
                NVIGI_LOG_ERROR("Failed to parse upload response - %s", e.what());
                return kResultNetServerError;
            }
        }

        return res;
    }

    CURL* curl{};
    std::string gfnKey{};
    bool verboseMode = false;
    bool currentRedactAuthFlag = true; // Current redaction flag for CURL debug callback
    inline static Network* s_interface = {};
};

INetworkInternal* getInterface()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        Network::s_interface = new Network();
    });
    return Network::s_interface;
}

void destroyInterface()
{
    if (Network::s_interface)
    {
        Network::s_interface->shutdown();
        delete Network::s_interface;
        Network::s_interface = {};
    }
}

}
}
