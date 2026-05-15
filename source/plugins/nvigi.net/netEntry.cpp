// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <sstream>
#include <atomic>
#include <future>
#include <unordered_set>

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.plugin/plugin.h"
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.extra/extra.h"
#include "external/json/source/nlohmann/json.hpp"
#include "_artifacts/gitVersion.h"
#include "source/plugins/nvigi.net/versions.h"
#include "source/plugins/nvigi.net/net.h"

using json = nlohmann::json;

namespace nvigi
{

namespace net
{
struct NetContext
{
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(NetContext);
    void onCreateContext() {};
    void onDestroyContext() {};

    INet api{};
};

}

NVIGI_PLUGIN_DEFINE("nvigi.plugin.net", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(API_MAJOR, API_MINOR, API_PATCH), net, NetContext)

using namespace net;

//! Public facing interface
//! 
Result _setVerboseMode(bool flag)
{
    return net::getInterface()->setVerboseMode(flag);
}
Result _setAuthToken(const char* token)
{
    return net::getInterface()->setAuthToken(token);
}
Result _httpGet(const Parameters& params, types::string& response)
{
    std::string data;
    auto res = net::getInterface()->httpGet(params, data);
    response = data.c_str();
    return res;
}
Result _httpPost(const Parameters& params, types::string& response)
{
    std::string data;
    auto res = net::getInterface()->httpPost(params, data);
    response = data.c_str();
    return res;
}
Result _httpGetRaw(const Parameters& params, types::vector<uint8_t>& response)
{
    std::string data;
    auto res = net::getInterface()->httpGet(params, data);
    response.resize(data.size());
    if (!data.empty()) memcpy(response.data(), data.data(), data.size());
    return res;
}
Result _httpPostRaw(const Parameters& params, types::vector<uint8_t>& response)
{
    // Raw methods must never enter the JSON parse / status polling path
    Parameters rawParams = params;
    rawParams.enableStatusPolling = false;
    std::string data;
    auto res = net::getInterface()->httpPost(rawParams, data);
    response.resize(data.size());
    if (!data.empty()) memcpy(response.data(), data.data(), data.size());
    return res;
}
Result _httpPostStreaming(const Parameters& params, StreamingDataCallback callback, void* userdata)
{
    return net::getInterface()->httpPostStreaming(params, callback, userdata);
}
Result _uploadAsset(const types::string& contentType, const types::string& description, const types::vector<uint8_t>& asset, types::string& assetId)
{
    return net::getInterface()->uploadAsset(contentType, description, asset, assetId);
}

namespace net
{
Result setVerboseMode(bool flag)
{
    NVIGI_CATCH_EXCEPTION(_setVerboseMode(flag));
}
Result setAuthToken(const char* token)
{
    NVIGI_CATCH_EXCEPTION(_setAuthToken(token));
}
Result httpGet(const Parameters& params, types::string& response)
{
    NVIGI_CATCH_EXCEPTION(_httpGet(params, response));
}
Result httpPost(const Parameters& params, types::string& response)
{
    NVIGI_CATCH_EXCEPTION(_httpPost(params, response));
}
Result httpPostStreaming(const Parameters& params, StreamingDataCallback callback, void* userdata)
{
    NVIGI_CATCH_EXCEPTION(_httpPostStreaming(params, callback, userdata));
}
Result uploadAsset(const types::string& contentType, const types::string& description, const types::vector<uint8_t>& asset, types::string& assetId)
{
    NVIGI_CATCH_EXCEPTION(_uploadAsset(contentType, description, asset, assetId));
}
Result httpGetRaw(const Parameters& params, types::vector<uint8_t>& response)
{
    NVIGI_CATCH_EXCEPTION(_httpGetRaw(params, response));
}
Result httpPostRaw(const Parameters& params, types::vector<uint8_t>& response)
{
    NVIGI_CATCH_EXCEPTION(_httpPostRaw(params, response));
}
} // net

//! Main entry point - get information about our plugin
//! 
Result nvigiPluginGetInfo(nvigi::framework::IFramework* framework, nvigi::plugin::PluginInfo** _info)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;
    
    // Internal API, we know that incoming pointer is always valid
    auto& info = plugin::getContext()->info;
    *_info = &info;

    info.id = plugin::net::kId;
    info.description = "networking protocols implementation";
    info.author = "NVIDIA";
    info.build = GIT_BRANCH_AND_LAST_COMMIT;
    info.interfaces = { plugin::getInterfaceInfo<INet>()};

    //! Defaults indicate no restrictions - plugin can run on any system, even without any adapter
    info.requiredVendor = nvigi::VendorId::eNone;
    info.minDriver = {};
    info.minOS = { NVIGI_DEF_MIN_OS_MAJOR, NVIGI_DEF_MIN_OS_MINOR, NVIGI_DEF_MIN_OS_BUILD };
    info.minGPUArch = {};
    return kResultOk;
}

//! Main entry point - starting our plugin
//! 
//! IMPORTANT: Plugins are started based on their priority.
//!
Result nvigiPluginRegister(framework::IFramework* framework)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    auto& ctx = (*net::getContext());

    if (net::getInterface()->initialize() == kResultOk)
    {
        ctx.api.setVerboseMode = net::setVerboseMode;
        ctx.api.nvcfSetToken = net::setAuthToken;
        ctx.api.nvcfGet = net::httpGet;
        ctx.api.nvcfPost = net::httpPost;
        ctx.api.nvcfPostStreaming = net::httpPostStreaming;
        ctx.api.nvcfUploadAsset = net::uploadAsset;
        ctx.api.httpGetRaw = net::httpGetRaw;
        ctx.api.httpPostRaw = net::httpPostRaw;
        framework->addInterface(plugin::net::kId, &ctx.api, 0);
    }

    return kResultOk;
}

//! Main exit point - shutting down our plugin
//! 
Result nvigiPluginDeregister()
{
    auto& ctx = (*net::getContext());
    net::getInterface()->shutdown();
    return kResultOk;
}

//! The only exported function - gateway to all functionality
NVIGI_EXPORT void* nvigiPluginGetFunction(const char* functionName)
{
    //! Core API
    NVIGI_EXPORT_FUNCTION(nvigiPluginGetInfo);
    NVIGI_EXPORT_FUNCTION(nvigiPluginRegister);
    NVIGI_EXPORT_FUNCTION(nvigiPluginDeregister);

    return nullptr;
}

}
