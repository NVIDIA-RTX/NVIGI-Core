// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <stddef.h> // size_t for Linux

#include "nvigi_struct.h"
#include "nvigi_result.h"
#include "nvigi_version.h"
#include "nvigi_helpers.h"

namespace nvigi
{

//! Vendor types
//! 
enum class VendorId : uint32_t
{
    // NOT physical adapter types: Used for indicating a "required adapter" for a component
    eAny = 0, // can be any valid display/compute adapter for the platform (e.g. DXGI on Windows)
    eNone = 1, // no adapter of any kind is needed (e.g. "headless/server")

    // Physical adapter types (based upon DXGI VendorId)
    eMS = 0x1414, // Microsoft Software Render Adapter
    eNVDA = 0x10DE,
    eAMD = 0x1002,
    eIntel = 0x8086,
};

//! Engine types
//! 
enum class EngineType : uint32_t
{
    eCustom,
    eUnreal,
    eUnity,
    eCount
};

//! Application Info
//!
//! OPTIONAL - can be chained with Preferences before calling nvigiInit
//! 
//! {BC5449C4-0096-408D-9C5E-4AE573A27A25}
struct alignas(8) AppInfo {
    AppInfo() {}; 
    NVIGI_UID(UID({ 0xbc5449c4, 0x0096, 0x408d,{ 0x9c, 0x5e, 0x4a, 0xe5, 0x73, 0xa2, 0x7a, 0x25 } }), kStructVersion1)
    //! Optional - Id provided by NVIDIA, if not specified then engine type and version are required
    uint32_t applicationId {};
    //! Optional - Type of the rendering engine used, if not specified then applicationId is required
    EngineType engine = EngineType::eCustom;
    //! Optional - Version of the rendering engine used
    const char* engineVersion{};
    //! Optional - GUID (like for example 'a0f57b54-1daf-4934-90ae-c4035c19df04')
    const char* projectId{};
    
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(AppInfo)

// If host did not include <winnt> already
#ifndef _WINNT_
//
// Locally Unique Identifier
//
typedef struct _LUID {
    unsigned long LowPart;
    long HighPart;
} LUID, * PLUID;
#endif

//! Interface 'AdapterSpec'
//!
//! {14F70C3F-9D6A-41E8-ABB2-9D15F7F83E5C}
struct alignas(8) AdapterSpec
{
    AdapterSpec() { };
    NVIGI_UID(UID({ 0x14f70c3f, 0x9d6a, 0x41e8,{0xab, 0xb2, 0x9d, 0x15, 0xf7, 0xf8, 0x3e, 0x5c} }), kStructVersion1)

    LUID id{};
    VendorId vendor{};
    size_t dedicatedMemoryInMB{}; // not shared with CPU

    //! Valid only for VendorId::eNVDA
    Version driverVersion{};
    uint32_t architecture{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(AdapterSpec)

//! Interface 'PluginSpec'
//!
//! {F997FBB5-9862-482E-929C-ADF8974E3645}
struct alignas(8) PluginSpec
{
    PluginSpec() { };
    NVIGI_UID(UID({ 0xf997fbb5, 0x9862, 0x482e,{0x92, 0x9c, 0xad, 0xf8, 0x97, 0x4e, 0x36, 0x45} }), kStructVersion2)

    PluginID id{};
    const char* pluginName{};
    Version pluginVersion{};
    Version pluginAPI{};
    Version requiredOSVersion{}; // NOTE: Ubuntu for Linux
    Version requiredAdapterDriverVersion{};
    VendorId requiredAdapterVendor{};
    uint32_t requiredAdapterArchitecture{};

    //! kResultOk if supported, specific error otherwise
    Result status{};

    // v2
    const UID* supportedInterfaces{};
    size_t numSupportedInterfaces{};

    //! v3+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(PluginSpec)

//! System bit flags
//! 
enum class SystemFlags : uint64_t
{
    eNone = 0x00,
    eHWSchedulingEnabled = 0x01
};

NVIGI_ENUM_OPERATORS_64(SystemFlags);

//! Interface 'PluginAndSystemInformation'
//!
//! NOTE: All allocations are managed by 'nvigi.core.framework'
//! and are valid until nvigiShutdown is called.
//! 
//! {EAFD9312-13FA-4DBD-9C05-1B43FD797F74}
struct alignas(8) PluginAndSystemInformation
{
    PluginAndSystemInformation() { };
    NVIGI_UID(UID({ 0xeafd9312, 0x13fa, 0x4dbd,{0x9c, 0x05, 0x1b, 0x43, 0xfd, 0x79, 0x7f, 0x74} }), kStructVersion1)
        
    size_t numDetectedPlugins{};
    const PluginSpec** detectedPlugins{};
    
    size_t numDetectedAdapters{};
    const AdapterSpec** detectedAdapters{};

    Version osVersion{};
    SystemFlags flags{};

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(PluginAndSystemInformation)


//! "Getters" for various plugin and interface related information
//! 

// HW scheduling is required for optimal performance when using CUDA and graphics (D3D/Vulkan)
inline bool isHWSchedulingEnabled(const PluginAndSystemInformation* info) { return uint64_t(info->flags & SystemFlags::eHWSchedulingEnabled) != 0; }

// Returns the state of an interface, if exported by a given plugin
inline Result isPluginExportingInterface(const PluginAndSystemInformation* info, const PluginID& id, const UID& _interface)
{
    for (size_t i = 0; i < info->numDetectedPlugins; i++)
    {
        auto plugin = info->detectedPlugins[i];
        if (plugin->id == id)
        {
            for (size_t k = 0; k < plugin->numSupportedInterfaces; k++)
            {
                if (plugin->supportedInterfaces[k] == _interface) return kResultOk;
            }
            return kResultNoImplementation;
        }
    }
    return kResultItemNotFound;
}

// Returns the state of a plugin, if found during the initialization process
inline Result getPluginStatus(const PluginAndSystemInformation* info, const PluginID& id)
{
    for (size_t i = 0; i < info->numDetectedPlugins; i++)
    {
        auto plugin = info->detectedPlugins[i];
        if (plugin->id == id) return plugin->status;
    }
    return kResultItemNotFound;
}

// Returns the name of a plugin, if found during the initialization process
inline Result getPluginName(const PluginAndSystemInformation* info, const PluginID& id, const char*& name)
{
    for (size_t i = 0; i < info->numDetectedPlugins; i++)
    {
        auto plugin = info->detectedPlugins[i];
        if (plugin->id == id)
        {
            name = plugin->pluginName;
            return kResultOk;
        }
    }
    return kResultItemNotFound;
}

// Returns the required OS version for a plugin, if found during the initialization process
inline Result getPluginRequiredOSVersion(const PluginAndSystemInformation* info, const PluginID& id, Version& version)
{
    for (size_t i = 0; i < info->numDetectedPlugins; i++)
    {
        auto plugin = info->detectedPlugins[i];
        if (plugin->id == id)
        {
            version = plugin->requiredOSVersion;
            return kResultOk;
        }
    }
    return kResultItemNotFound;
}

// Returns the required adapter driver version for a plugin, if found during the initialization process
inline Result getPluginRequiredAdapterDriverVersion(const PluginAndSystemInformation* info, const PluginID& id, Version& version)
{
    for (size_t i = 0; i < info->numDetectedPlugins; i++)
    {
        auto plugin = info->detectedPlugins[i];
        if (plugin->id == id)
        {
            version = plugin->requiredAdapterDriverVersion;
            return kResultOk;
        }
    }
    return kResultItemNotFound;
}

// Returns the required adapter vendor for a plugin, if found during the initialization process
inline Result getPluginRequiredAdapterVendor(const PluginAndSystemInformation* info, const PluginID& id, VendorId& vendor)
{
    for (size_t i = 0; i < info->numDetectedPlugins; i++)
    {
        auto plugin = info->detectedPlugins[i];
        if (plugin->id == id)
        {
            vendor = plugin->requiredAdapterVendor;
            return kResultOk;
        }
    }
    return kResultItemNotFound;
}

// Returns the required adapter architecture for a plugin, if found during the initialization process
inline Result getPluginRequiredAdapterArchitecture(const PluginAndSystemInformation* info, const PluginID& id, uint32_t& arch)
{
    for (size_t i = 0; i < info->numDetectedPlugins; i++)
    {
        auto plugin = info->detectedPlugins[i];
        if (plugin->id == id)
        {
            arch = plugin->requiredAdapterArchitecture;
            return kResultOk;
        }
    }
    return kResultItemNotFound;
}

}