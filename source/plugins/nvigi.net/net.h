// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <string>
#include <vector>

#include "source/core/nvigi.api/nvigi_struct.h"
#include "source/core/nvigi.types/types.h"
#include "source/plugins/nvigi.net/nvigi_net.h"
#include "external/json/source/nlohmann/json.hpp"
using json = nlohmann::json;

namespace nvigi
{

namespace net
{

// {8560A124-99B4-4ED8-89FE-4406EF08CB30}
struct alignas(8) Parameters {
    Parameters() {}; 
    NVIGI_UID(UID({ 0x8560a124, 0x99b4, 0x4ed8,{ 0x89, 0xfe, 0x44, 0x6, 0xef, 0x8, 0xcb, 0x30 } }), kStructVersion3);
    //! IMPORTANT: Using nvigi::types ABI stable implementations
    //! 
    types::string url{};
    types::vector<types::string> headers{};
    types::vector<uint8_t> data{};
    
    //! v2 - Security and resource management (matches RESTParameters from nvigi_cloud.h)
    //! See nvigi_cloud.h RESTParameters for detailed documentation
    bool verifySSLPeer = true;
    bool verifySSLHost = true;
    types::string sslCABundlePath{};
    uint32_t timeoutSeconds = 300;
    uint32_t connectionTimeoutSeconds = 30;
    uint32_t lowSpeedLimit = 1024;
    uint32_t lowSpeedTimeSeconds = 60;
    uint64_t maxResponseSizeBytes = 104857600;
    uint64_t maxStreamingSizeBytes = 104857600;
    bool allowHTTP = false;
    bool followRedirects = false;
    uint32_t maxRedirects = 5;
    bool redactAuthInLogs = true;

    //! v3 - Generic HTTP support
    //! Per-request Bearer auth token. If non-empty, overrides the global token set via setAuthToken.
    types::string authToken{};
    //! When true, enables status polling ("pending-evaluation" loop) after POST.
    //! Default is false for generic HTTP usage.
    bool enableStatusPolling = false;
    //! Base URL for status polling requests. The request ID is appended as a path segment.
    //! If empty and enableStatusPolling is true, defaults to "https://api.nvcf.nvidia.com/v2/nvcf/exec/status/".
    types::string statusPollingBaseUrl{};
};

NVIGI_VALIDATE_STRUCT(Parameters)

// C-style callback function pointer for streaming
typedef size_t(*StreamingDataCallback)(const char* data, size_t size, void* userdata);

// {E70C7C30-5E61-4F3A-B40F-A6F561EDB563}
struct alignas(8) INet {
    INet() {};
    NVIGI_UID(UID({ 0xe70c7c30, 0x5e61, 0x4f3a,{ 0xb4, 0xf, 0xa6, 0xf5, 0x61, 0xed, 0xb5, 0x63 } }), kStructVersion3);
    Result(*setVerboseMode)(bool flag);
    //! NOTE: The nvcf* names are historical and kept for ABI compatibility.
    //! These are generic HTTP methods — NVCF-specific behavior (e.g. status polling)
    //! is controlled via Parameters fields, not baked into the methods themselves.
    Result(*nvcfSetToken)(const char* token);
    Result(*nvcfGet)(const Parameters& params, types::string& response);
    Result(*nvcfPost)(const Parameters& params, types::string& response);
    Result(*nvcfUploadAsset)(const types::string& contentType, const types::string& description, const types::vector<uint8_t>& asset, types::string& assetId);

    // v2
    Result(*nvcfPostStreaming)(const Parameters& params, StreamingDataCallback callback, void* userdata);

    // v3 - Raw response methods for non-JSON payloads (e.g. audio, binary data)
    Result(*httpGetRaw)(const Parameters& params, types::vector<uint8_t>& response);
    Result(*httpPostRaw)(const Parameters& params, types::vector<uint8_t>& response);
};

NVIGI_VALIDATE_STRUCT(INet)

struct INetworkInternal
{
    virtual Result initialize() = 0;
    virtual Result shutdown() = 0;
    virtual Result setVerboseMode(bool flag) = 0;
    virtual Result setAuthToken(const char* token) = 0;
    virtual Result httpGet(const Parameters& params, std::string& response) = 0;
    virtual Result httpPost(const Parameters& params, std::string& response) = 0;
    virtual Result httpPostStreaming(const Parameters& params, StreamingDataCallback callback, void* userdata) = 0;
    virtual Result uploadAsset(const types::string& contentType, const types::string& description, const types::vector<uint8_t>& asset, types::string& assetId) = 0;
};

INetworkInternal* getInterface();
void destroyInterface();

}
}