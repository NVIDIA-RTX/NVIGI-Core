// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.extra/extra.h"
#include "source/core/nvigi.types/types.h"

namespace nvigi
{
namespace core
{
namespace framework
{
constexpr PluginID kId = { {0x42534bcf, 0x590c, 0x40a9,{0x80, 0x83, 0x65, 0x3f, 0x97, 0x8c, 0xcd, 0x1b}}, 0x1a8307 };  // {42534BCF-590C-40A9-8083-653F978CCD1B} [nvigi.core.framework]
}
}

namespace framework
{

using InterfaceFlags = uint32_t;
constexpr InterfaceFlags InterfaceFlagNone = 0x0;
constexpr InterfaceFlags InterfaceFlagNotRefCounted = 0x01;

//! Internal interface
//! 
//! {0F688505-89E4-45FF-84E8-D08380592BD0}
struct alignas(8) IFramework {
    IFramework() {};
    NVIGI_UID(UID({ 0xf688505, 0x89e4, 0x45ff,{ 0x84, 0xe8, 0xd0, 0x83, 0x80, 0x59, 0x2b, 0xd0 } }), kStructVersion1)
    bool (*addInterface)(PluginID feature, void* _interface, InterfaceFlags flags);
    void* (*getInterface)(PluginID feature, const UID& type, uint32_t version, const char* utf8PathToPlugins);
    bool (*releaseInterface)(PluginID feature, const UID& type);
    size_t (*getNumInterfaces)(PluginID feature);
    types::string(*getModelDirectoryForPlugin)(PluginID feature);
    types::string(*getUTF8PathToDependencies)();
    PluginID(*getPluginIdFromName)(const char* _name);
};

NVIGI_VALIDATE_STRUCT(IFramework)

template<typename T>
bool getInterface(IFramework* framework, PluginID feature, T** _interface, const char* utf8PathToPlugins = nullptr)
{
    if (!_interface) return false;
    T tmp{};
    *_interface = static_cast<T*>(framework->getInterface(feature, T::s_type, tmp.getVersion(), utf8PathToPlugins));
    if (*_interface == nullptr)
    {
        NVIGI_LOG_ERROR("Failed to obtain interface '%s'", extra::guidToString(T::s_type).c_str());
    }
    return *_interface != nullptr;
}

template<typename T>
bool releaseInterface(IFramework* framework, PluginID feature, T* _interface)
{
    if (!_interface) return false;
    return framework->releaseInterface(feature, T::s_type);
}
}
}
