// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_types.h"

#ifdef _WIN32
#define NVIGI_API extern "C"
#else
#define NVIGI_API extern "C" __attribute__((visibility("default")))
#endif
#define NVIGI_CHECK(f) {auto _r = f; if(_r != nvigi::kResultOk) return _r;}
#define NVIGI_FAILED(r, f) nvigi::Result r = f; r != nvigi::kResultOk
#define NVIGI_SUCCEEDED(r, f) nvigi::Result r = f; r == nvigi::kResultOk
#define NVIGI_FUN_DECL(name) PFun_##name* name{}

namespace nvigi {

//! Different levels for logging
enum class LogLevel : uint32_t
{
    //! No logging
    eOff,
    //! Default logging
    eDefault,
    //! Verbose logging
    eVerbose,
    //! Total count
    eCount
};

//! Log type
enum class LogType : uint32_t
{
    //! Controlled by LogLevel, NVIGI can show more information in eLogLevelVerbose mode
    eInfo,
    //! Always shown regardless of LogLevel
    eWarn,
    eError,
    //! Total count
    eCount
};

//! Logging callback
//!
//! Use these callbacks to track messages posted in the log.
//! If any of the NVIGI methods returns false use eLogTypeError
//! type to track down what went wrong and why.
using PFun_LogMessageCallback = void(LogType type, const char* msg);

//! Optional flags
enum class PreferenceFlags : uint64_t
{
    //! Optional - Enables downloading of Over The Air (OTA) updates for NVIGI
    //!
    //! This will invoke the OTA updater to look for new updates.
    eAllowOTA = 1 << 0,
    //! Optional - Disables automatic process privilege downgrade when calling NVIGI functions
    //!
    //! If host process is running with elevated privileges NVIGI will try to downgrade them as needed.
    //! Setting this flag will override this behavior hence potentially introducing security vulnerability.
    eDisablePrivilegeDowngrade = 1 << 1,
    //! Optional - Disables higher resolution timer frequency
    //!
    //! For optimal timing performance NVIGI adjusts CPU timer resolution frequency. 
    //! Set this flag to opt out and leave it unchanged.
    eDisableCPUTimerResolutionChange = 1 << 2
};

NVIGI_ENUM_OPERATORS_64(PreferenceFlags)

//! Application preferences
//!
//! {1CA10965-BF8E-432B-8DA1-6716D879FB14}
struct alignas(8) Preferences {
    Preferences() {}; 
    NVIGI_UID(UID({ 0x1ca10965, 0xbf8e, 0x432b,{ 0x8d, 0xa1, 0x67, 0x16, 0xd8, 0x79, 0xfb, 0x14 } }), kStructVersion1)
    //! Optional - In non-production builds it is useful to enable debugging console window
    bool showConsole = false;
    //! Optional - Various logging levels
    LogLevel logLevel = LogLevel::eDefault;
    //! Optional - Paths to locations where to look for plugins and their dependencies
    //!
    //! NOTE: Duplicated plugins or dependencies are NOT allowed
    const char** utf8PathsToPlugins{};
    //! Optional - Number of paths to search
    uint32_t numPathsToPlugins = 0;
    //! Optional - Path to the location where logs and other data should be stored
    //! 
    //! NOTE: Set this to nullptr in order to disable logging to a file
    const char* utf8PathToLogsAndData{};
    //! Optional - Allows log message tracking including critical errors if they occur
    PFun_LogMessageCallback* logMessageCallback{};
    //! Optional - Flags used to enable or disable advanced options
    PreferenceFlags flags{};
    //! Optional - Path to the location where to look for plugin dependencies
    //!
    //! NOTE: If not provided NVIGI will assume that dependencies are next to the plugin(s)
    //! and that there are NO shared dependencies since they cannot be duplicated.
    const char* utf8PathToDependencies{};

    //! IMPORTANT: New members go here or if optional can be chained in a new struct, see nvigi_struct.h for details
};

NVIGI_VALIDATE_STRUCT(Preferences)

struct BaseStructure;
}

//! In-Game Inferencing (NVIGI) core API functions (check feature specific headers for additional APIs)
//! 
using PFun_nvigiInit = nvigi::Result(const nvigi::Preferences& pref, nvigi::PluginAndSystemInformation** pluginInfo, uint64_t sdkVersion);
using PFun_nvigiShutdown = nvigi::Result();
using PFun_nvigiLoadInterface = nvigi::Result(nvigi::PluginID feature, const nvigi::UID& interfaceType, uint32_t interfaceVersion, void** _interface, const char* utf8PathToPlugin);
using PFun_nvigiUnloadInterface = nvigi::Result(nvigi::PluginID feature, void* _interface);

//! Initializes the NVIGI framework
//!
//! Call this method when your application is initializing
//!
//! @param pref Specifies preferred behavior for the NVIGI framework (NVIGI will keep a copy)
//! @param pluginInfo Optional pointer to data structure containing information about plugins, user system
//! @param sdkVersion Current SDK version
//! @returns nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiInit(const nvigi::Preferences &pref, nvigi::PluginAndSystemInformation** pluginInfo = nullptr, uint64_t sdkVersion = nvigi::kSDKVersion);

//! Shuts down the NVIGI module
//!
//! Call this method when your application is shutting down. 
//!
//! @returns nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiShutdown();

//! Loads an interface for a specific NVIGI feature
//!
//! Call this method when specific interface is needed.
//!
//! NOTE: Interfaces are reference counted so they all must be released before underlying plugin is released.
//!
//! @param feature Specifies feature which needs to provide the requested interface
//! @param interfaceType Type of the interface to obtain
//! @param interfaceVersion Minimal version of the interface to obtain
//! @param interface Pointer to the interface
//! @param utf8PathToPlugin Optional path to a new plugin which provides this interface
//! @returns nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! NOTE: It is recommended to use the template based helpers `nvigiGetInterface` or `nvigiGetInterfaceDynamic` (see below in this header)
//! 
//! For example:
//! 
//! nvigi::IGeneralPurposeTransformer* igpt{};
//! 
//! // Static linking
//! nvigiGetInterface(nvigi::plugin::gpt::ggml::cuda::kId, &igpt);
//! 
//! // Dynamic `nvigi.core.framework` loading
//! nvigiGetInterfaceDynamic(nvigi::plugin::gpt::ggml::cuda::kId, &igpt, nvigiLoadInterfaceFunction);
//! 
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiLoadInterface(nvigi::PluginID feature, const nvigi::UID& interfaceType, uint32_t interfaceVersion, void** _interface, const char* utf8PathToPlugin);

//! Unloads an interface for a specific NVIGI feature
//!
//! Call this method when specific interface is no longer needed
//!
//! NOTE: Interfaces are reference counted so they all must be released before underlying plugin is released.
//!
//! @param feature Specifies feature which provided the interface
//! @param interface Pointer to the interface
//! @returns nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
//!
//! This method is NOT thread safe.
NVIGI_API nvigi::Result nvigiUnloadInterface(nvigi::PluginID feature, void* _interface);

//! Helper method when statically linking NVIGI framework
//! 
template<typename T>
inline nvigi::Result nvigiGetInterface(nvigi::PluginID feature, T** _interface, const char* utf8PathToPlugin = nullptr)
{
    void* tmp{};
    T t{};
    if (NVIGI_FAILED(result, nvigiLoadInterface(feature, T::s_type, t.getVersion(), &tmp, utf8PathToPlugin)))
    {
        return result;
    }
    *_interface = static_cast<T*>(tmp);
    return nvigi::kResultOk;
}

//! Helper method when dynamically loading NVIGI framework
//! 
template<typename T>
inline nvigi::Result nvigiGetInterfaceDynamic(nvigi::PluginID feature, T** _interface, PFun_nvigiLoadInterface* func, const char* utf8PathToPlugin = nullptr)
{
    void* tmp{};
    T t{};
    if (NVIGI_FAILED(result, func(feature, T::s_type, t.getVersion(), &tmp, utf8PathToPlugin)))
    {
        return result;
    }
    *_interface = static_cast<T*>(tmp);
    return nvigi::kResultOk;
}