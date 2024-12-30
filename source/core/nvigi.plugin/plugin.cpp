// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/core/nvigi.plugin/plugin.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.types/types.h"
#include "source/core/nvigi.framework/framework.h"

namespace nvigi
{

namespace log
{
ILog* s_log{};
ILog* getInterface() { return s_log; }
}

namespace memory
{
IMemoryManager* s_mm{};
IMemoryManager* getInterface() { return s_mm; }
}

namespace exception
{
IException* s_exception{};
IException* getInterface() { return s_exception; }
}

namespace system
{
ISystem* s_system{};
ISystem* getInterface() { return s_system; }
}

namespace plugin
{

bool internalPluginSetup(nvigi::framework::IFramework* framework)
{
    auto ctx = plugin::getContext();

    if (!framework::getInterface(framework, nvigi::core::framework::kId, &exception::s_exception)) return false;
    if (!framework::getInterface(framework, nvigi::core::framework::kId, &memory::s_mm)) return false;
    if (!framework::getInterface(framework, nvigi::core::framework::kId, &log::s_log)) return false;
    if (!framework::getInterface(framework, nvigi::core::framework::kId, &system::s_system)) return false;

    ctx->framework = framework;

#ifndef NVIGI_PRODUCTION
    try
    {
        //! Search for "nvigi.plugin.$(plugin_name).json" with extra settings
    
        assert(ctx->pluginName.find("nvigi.plugin.") == 0);
        std::vector<std::wstring> jsonLocations = { nvigi::file::getExecutablePath(), nvigi::file::getModulePath(), nvigi::file::getCurrentDirectoryPath() };
        for (auto& jsonPath : jsonLocations)
        {
            std::wstring extraJSONFile = (jsonPath + std::wstring(L"/") + extra::toWStr(ctx->pluginName.c_str()) + L".json").c_str();
            if (file::exists(extraJSONFile.c_str()))
            {
                NVIGI_LOG_INFO("Found extra JSON config %S", extraJSONFile.c_str());
                auto jsonText = file::read(extraJSONFile.c_str());
                if (!jsonText.empty())
                {
                    // safety null in case the JSON string is not null-terminated (found by AppVerif)
                    jsonText.push_back(0);
                    std::istringstream stream((const char*)jsonText.data());
                    ctx->pluginConfig = new json();
                    json& pluginConfig = *static_cast<json*>(ctx->pluginConfig);
                    stream >> pluginConfig;
                }
                break;
            }
        }
    }
    catch (std::exception& e)
    {
        NVIGI_LOG_ERROR("JSON exception %s", e.what());
    };
#endif

    return true;
}

}
}
