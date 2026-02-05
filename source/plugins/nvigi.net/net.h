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
    NVIGI_UID(UID({ 0x8560a124, 0x99b4, 0x4ed8,{ 0x89, 0xfe, 0x44, 0x6, 0xef, 0x8, 0xcb, 0x30 } }), kStructVersion1);
    //! IMPORTANT: Using nvigi::types ABI stable implementations
    //! 
    types::string url{};
    types::vector<types::string> headers{};
    types::vector<uint8_t> data{};
};

NVIGI_VALIDATE_STRUCT(Parameters)

// C-style callback function pointer for streaming
typedef size_t(*StreamingDataCallback)(const char* data, size_t size, void* userdata);

// {E70C7C30-5E61-4F3A-B40F-A6F561EDB563}
struct alignas(8) INet {
    INet() {};
    NVIGI_UID(UID({ 0xe70c7c30, 0x5e61, 0x4f3a,{ 0xb4, 0xf, 0xa6, 0xf5, 0x61, 0xed, 0xb5, 0x63 } }), kStructVersion2);
    Result(*setVerboseMode)(bool flag);
    Result(*nvcfSetToken)(const char* token);
    Result(*nvcfGet)(const Parameters& params, types::string& response);
    Result(*nvcfPost)(const Parameters& params, types::string& response);
    Result(*nvcfUploadAsset)(const types::string& contentType, const types::string& description, const types::vector<uint8_t>& asset, types::string& assetId);

    // v2
    Result(*nvcfPostStreaming)(const Parameters& params, StreamingDataCallback callback, void* userdata);
};

NVIGI_VALIDATE_STRUCT(INet)

struct INetworkInternal
{
    virtual Result initialize() = 0;
    virtual Result shutdown() = 0;
    virtual Result setVerboseMode(bool flag) = 0;
    virtual Result nvcfSetToken(const char* token) = 0;
    virtual Result nvcfGet(const Parameters& params, json& response) = 0;
    virtual Result nvcfPost(const Parameters& params, json& response) = 0;
    virtual Result nvcfPostStreaming(const Parameters& params, StreamingDataCallback callback, void* userdata) = 0;
    virtual Result nvcfUploadAsset(const types::string& contentType, const types::string& description, const types::vector<uint8_t>& asset, types::string& assetId) = 0;
};

INetworkInternal* getInterface();
void destroyInterface();

}
}