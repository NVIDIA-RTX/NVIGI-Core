// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.extra/extra.h"
#include "source/plugins/nvigi.net/net.h"
#define CURL_STATICLIB
#include "external/libcurl/include/curl/curl.h"
#include "external/json/source/nlohmann/json.hpp"

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

static size_t curlCallbackWriteStdString(void* contents, size_t size, size_t nmemb, std::string* s)
{
    size_t newLength = size * nmemb;
    try
    {
        s->append((char*)contents, newLength);
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

// Custom debug function
int curlCallbackDebug(CURL* handle, curl_infotype type, char* data, size_t size, void* userptr) 
{
    // Note: This gets called ONLy if CURLOPT_VERBOSE is set to 1 which is controlled via runtime parameters

    if (type == CURLINFO_TEXT) {
        NVIGI_LOG("curl",LogType::eInfo,log::ConsoleForeground::WHITE,"%s", std::string(data, size).c_str());
    }
    else if (type == CURLINFO_HEADER_IN) {
        NVIGI_LOG("curl][header-in", LogType::eInfo, log::ConsoleForeground::WHITE, "%s", std::string(data, size).c_str());
    }
    else if (type == CURLINFO_HEADER_OUT) {
        NVIGI_LOG("curl][header-out", LogType::eInfo, log::ConsoleForeground::WHITE, "%s", std::string(data, size).c_str());
    }
    else if (type == CURLINFO_DATA_IN) {
        NVIGI_LOG("curl][data-in", LogType::eInfo, log::ConsoleForeground::WHITE, "%s", std::string(data, size).c_str());
    }
    else if (type == CURLINFO_DATA_OUT) {
        NVIGI_LOG("curl][data-out", LogType::eInfo, log::ConsoleForeground::WHITE, "%s", std::string(data, size).c_str());
    }
    return 0; // Return 0 to indicate success
}

// Structure to pass both callback and userdata to CURL
struct CallbackData {
    StreamingDataCallback callback;
    void* userdata;
};

// Create a custom write callback for streaming data
size_t streamingWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* cbData = static_cast<CallbackData*>(userp);
    size_t realsize = size * nmemb;
    return cbData->callback(static_cast<const char*>(contents), realsize, cbData->userdata);
};

struct Network : public INetworkInternal
{
    virtual Result initialize() override final
    {
        curl = curl_easy_init();
        if (!curl)
        {
            NVIGI_LOG_ERROR("Unable to initialize CURL library");
            return kResultNetFailedToInitializeCurl;
        }
        // Set the debug callback
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curlCallbackDebug);
        return kResultOk;
    }

    virtual Result shutdown() override final
    {
        if (curl)
        {
            curl_easy_cleanup(curl);
            curl = {};
        }
        return kResultOk;
    }

    virtual Result setVerboseMode(bool flag) override final
    {
        verboseMode = flag;
        return kResultOk;
    }

    virtual Result nvcfSetToken(const char* token) override final
    {
        gfnKey = token;
        return kResultOk;
    }

    virtual Result nvcfGet(const Parameters& params, json& response) override final
    {
        curl_easy_setopt(curl, CURLOPT_URL, params.url.c_str());

        struct curl_slist* headers = NULL;
        for (auto h : params.headers)
        {
            headers = curl_slist_append(headers, h.c_str());
        }

        if (!gfnKey.empty())
        {
            headers = curl_slist_append(headers, ("Authorization: Bearer " + gfnKey).c_str());
        }

        std::string tmp;
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallbackWriteStdString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &tmp);
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

        NVIGI_LOG_VERBOSE("CURL GET request returned - %s", tmp.c_str());

        try
        {
            response = json::parse(tmp);
        }
        catch (std::exception&)
        {
            NVIGI_LOG_WARN("Server returned non-JSON response '%s'", tmp.c_str());
            return kResultNetServerError;
        }
        return kResultOk;
    }

    virtual Result nvcfPost(const Parameters& params, json& response) override final
    {
        curl_easy_setopt(curl, CURLOPT_URL, params.url.c_str());

        struct curl_slist* headers = NULL;
        for (auto h : params.headers)
        {
            headers = curl_slist_append(headers, h.c_str());
        }
        
        if (!gfnKey.empty())
        {
            headers = curl_slist_append(headers, ("Authorization: Bearer " + gfnKey).c_str());
        }

        std::string tmp;
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.data.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, params.data.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallbackWriteStdString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &tmp);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, verboseMode ? 1 : 0);

        NVIGI_LOG_VERBOSE("Connecting to '%s', awaiting response ...", params.url.c_str());
        auto res = curl_easy_perform(curl);
        
        if (res != CURLE_OK)
        {
            NVIGI_LOG_ERROR("CURL POST request returned - %s", tmp.c_str());
            NVIGI_LOG_ERROR("CURL POST request failed with error - %s", curl_easy_strerror(res));
            curl_easy_reset(curl);
            curl_slist_free_all(headers);
            return kResultNetCurlError;
        }

        curl_easy_reset(curl);
        curl_slist_free_all(headers);

        try
        {
            response = json::parse(tmp);

            constexpr float kMaxWaitMs = 5000.0f;
            constexpr const char* kNvcfOK = "fulfilled";

            auto getStatus = [](json& response)->std::string
            {
                std::string status = kNvcfOK;
                if (response.contains("status"))
                {
                    if (response.at("status").is_string())
                    {
                        status = response.value("status", status);
                    }
                    else if (response.at("status").is_number_integer())
                    {
                        status = std::to_string(response.at("status").operator int());
                    }
                }
                else if (response.contains("error"))
                {
                    if (response.at("error").is_string())
                    {
                        status = response.value("error", status);
                    }
                    else if (response.at("error").is_number_integer())
                    {
                        status = std::to_string(response.at("error").operator int());
                    }
                }
                return status;
            };

            auto status = getStatus(response);
            auto tStart = std::chrono::high_resolution_clock::now();
            while (status == "pending-evaluation")
            {
                std::string reqId = response["reqId"];
                Parameters statusParams{};
                statusParams.url = ("https://api.nvcf.nvidia.com/v2/nvcf/exec/status/" + reqId).c_str();
                response.clear();
                if (nvcfGet(statusParams, response) != kResultOk) break;
                status = getStatus(response);
                std::chrono::duration<float, std::milli> tElapsed = std::chrono::high_resolution_clock::now() - tStart;
                if (tElapsed.count() > kMaxWaitMs)
                {
                    status = extra::format("timed out after {}ms", kMaxWaitMs);
                    break;
                }
            }
            if (status != kNvcfOK)
            {
                NVIGI_LOG_WARN("POST request failed, status '%s', details: '%s'", status.c_str(), tmp.c_str());
                return kResultNetServerError;
            }
        }
        catch (std::exception&)
        {
            // If we reach this point then CURL request succeeded but server returned an error in plain text format
            NVIGI_LOG_WARN("Server returned non-JSON response '%s'", tmp.c_str());
            return kResultNetServerError;
        }
        return kResultOk;
    }

    virtual Result nvcfPostStreaming(const Parameters& params, StreamingDataCallback callback, void* userdata) override final
    {
        curl_easy_setopt(curl, CURLOPT_URL, params.url.c_str());

        struct curl_slist* headers = NULL;
        for (auto h : params.headers)
        {
            headers = curl_slist_append(headers, h.c_str());
        }
        
        if (!gfnKey.empty())
        {
            headers = curl_slist_append(headers, ("Authorization: Bearer " + gfnKey).c_str());
        }

        CallbackData callbackData = { callback, userdata };

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

        curl_easy_reset(curl);
        curl_slist_free_all(headers);
        return kResultOk;
    }

    virtual Result nvcfUploadAsset(const types::string& contentType, const types::string& description, const types::vector<uint8_t>& asset, types::string& assetId) override final
    {
        // First we need to obtain asset id and URL from NVCF
        net::Parameters params;
        params.url = "https://api.nvcf.nvidia.com/v2/nvcf/assets";
        params.headers = { "Content-Type: application/json" };

        json data;
        data["contentType"] = contentType.c_str();
        data["description"] = description.c_str();

        std::string jsonObj = data.dump(-1, ' ', false, json::error_handler_t::replace);

        params.data.resize(jsonObj.size());
        memcpy(params.data.data(), jsonObj.c_str(), jsonObj.size());
        
        json response;
        auto res = nvcfPost(params, response);
        if (res == kResultOk)
        {
            // We got our id and url, now we can upload the data
            assetId = response["assetId"].operator std::string().c_str();
            std::string uploadUrl = response["uploadUrl"];

            NVIGI_LOG_VERBOSE("Uploaded asset id '%s'", assetId.c_str());

            MemoryStruct uploadData = { asset.data(), asset.size(), 0};

            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
            headers = curl_slist_append(headers, ("x-amz-meta-nvcf-asset-description: " + description).c_str());
            
            std::string tmp;
            curl_easy_setopt(curl, CURLOPT_URL, uploadUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_PUT, 1L);
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, curlCallbackReadBytes);
            curl_easy_setopt(curl, CURLOPT_READDATA, (void*)&uploadData);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)uploadData.size);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallbackWriteStdString);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &tmp);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, verboseMode ? 1 : 0);

            auto res = curl_easy_perform(curl);

            curl_easy_reset(curl);
            curl_slist_free_all(headers);

            NVIGI_LOG_VERBOSE("CURL request returned - %s", tmp.c_str());

            if (res != CURLE_OK)
            {
                NVIGI_LOG_ERROR("CURL request failed with error - %s", curl_easy_strerror(res));
                return kResultNetCurlError;
            }
        }

        return res;
    }

    CURL* curl{};
    std::string gfnKey{};
    bool verboseMode = false;
    inline static Network* s_interface = {};
};

INetworkInternal* getInterface()
{
    if (!Network::s_interface)
    {
        Network::s_interface = new Network();
    }
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
