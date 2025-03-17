// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#ifdef NVIGI_WINDOWS
#include <Windows.h>
#endif
#include <filesystem>
#include <unordered_set>

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.api/nvigi_version.h"
#include "source/core/nvigi.api/nvigi_security.h"
#include "source/core/nvigi.api/nvigi_types.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.memory/memory.h"
#include "source/core/nvigi.extra/extra.h"  
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.system/system.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.plugin/plugin.h"
#include "source/core/nvigi.framework/framework.h"
#include "source/core/nvigi.api/nvigi_version.h"
#include "external/json/source/nlohmann/json.hpp"
#include "_artifacts/gitVersion.h"

using json = nlohmann::json;

//! Forward declarations
//! 
nvigi::Result nvigiLoadInterfaceImpl(nvigi::PluginID feature, const nvigi::UID& type, uint32_t version, void** _interface, const char* utf8PathToPlugin);
nvigi::Result nvigiUnloadInterfaceImpl(nvigi::PluginID feature, const nvigi::UID& type);

//! Implemented in system.cpp, not in a header since we don't want accidental includes by plugins
namespace nvigi::system
{
extern bool getSystemCaps(nvigi::VendorId forceAdapterId, uint32_t forceArchitecture, nvigi::system::SystemCaps* info);
extern bool getOSVersionAndUpdateTimerResolution(nvigi::system::SystemCaps* caps);
extern void cleanup(nvigi::system::SystemCaps* caps);
extern void setTimerResolution();
extern bool validateDLL(const std::wstring& dllFilePath, const std::vector<std::wstring>& utf16DependeciesDirectories, std::map<std::string, fs::path>& dependencies);
extern void setPreferenceFlags(PreferenceFlags flags);
}

namespace nvigi
{

namespace framework
{

//! Not shared outside of framework
struct PluginInternals
{
    HMODULE hmod;
    plugin::PFun_PluginDeregister* pluginDeregister;
};

using ModulesMap = std::map<nvigi::PluginID, std::tuple<std::filesystem::path, PluginInternals>>;
using InterfacesMap = std::map<nvigi::PluginID, std::vector<std::tuple<int32_t, BaseStructure*, InterfaceFlags>>>;

struct FrameworkContext
{
    Version sdkVersion = { NVIGI_CORESDK_VERSION_MAJOR, NVIGI_CORESDK_VERSION_MINOR, NVIGI_CORESDK_VERSION_PATCH };
    Version apiVersion = { NVIGI_CORESDK_API_VERSION_MAJOR, NVIGI_CORESDK_API_VERSION_MINOR, NVIGI_CORESDK_API_VERSION_PATCH };
    Version hostSDKVersion{};
    nvigi::system::SystemCaps caps{};
    nvigi::framework::IFramework framework{};
    //! Always avoid static destruction hence these are on heap!
    ModulesMap modules{};
    InterfacesMap interfaces{};

    std::string utf8PathToPlugins{}; // internal, set only if JSON override is used
    std::string utf8PathToDependencies{}; // provided by host

    //! For reporting back via nvigi::PluginAndSystemInformation
    std::vector<const nvigi::PluginSpec*> pluginSpecs;
    std::vector<const nvigi::AdapterSpec*> adapterSpecs;

    PluginAndSystemInformation pluginSysInfo{};

    //! DLL validation
#ifndef NVIGI_PRODUCTION
    std::map<std::string, fs::path> dependencies{};
    std::vector<std::string> registerPlugins{};
    std::map<std::string, PluginID> nameToId{};
#endif
};
FrameworkContext* ctx;

std::string getPluginName(PluginID id)
{
    if (id == core::framework::kId) return "nvigi.core.framework";
    auto& [path, internals] = ctx->modules[id];
    return path.filename().replace_extension().string();
}

//! Internal framework API
//! 
//! Add interface for a give feature
//! 
bool addInterface(PluginID feature, void* _interface, InterfaceFlags flags)
{
    auto& list = ctx->interfaces[feature];
    for (auto& [refCount, i, flagsTmp] : list)
    {
        if (i->type == ((const nvigi::BaseStructure*)_interface)->type) return false;
    }
    list.push_back({ 0, (nvigi::BaseStructure*)_interface, flags });
    NVIGI_LOG_VERBOSE("[%s] added interface '%s'", getPluginName(feature).c_str(), extra::guidToString(((const nvigi::BaseStructure*)_interface)->type).c_str());
    return true;
}

//! Internal framework API
//! 
//! Release reference of an interface for a given feature
//! 
bool releaseInterface(PluginID feature, const UID& type)
{
    return nvigiUnloadInterfaceImpl(feature, type) == nvigi::kResultOk;
}

//! Internal framework API
//! 
//! Get interface for a give feature, if not available try to load correct plugin (if found)
//! 
void* getInterface(PluginID feature, const UID& type, uint32_t version, const char* utf8PathToPlugins)
{
    void* _interface{};
    // No interfaces registered for plugin yet, try loading the plugin which should have it    
    nvigiLoadInterfaceImpl(feature, type, version, &_interface, utf8PathToPlugins);
    return _interface;
}

//! Internal framework API
//! 
//! Get number of interfaces for a give feature
//! 
size_t getNumInterfaces(PluginID feature)
{
    return ctx->interfaces[feature].size();
}

//! Internal framework API
//! 
//! Returns name for the folder under "nvigi.models" where models for a given plugin are stored
//! 
types::string getModelDirectoryForPlugin(PluginID feature)
{
    //! Plugins are in format 'nvigi.plugin.$name.$backend.$api'
    //!
    //! Same models can be loaded by multiple backends so directory
    //! is in format `nvigi.plugin.$name.$backend' (assuming backend exists)
    //! 
    std::istringstream iss(getPluginName(feature));
    std::string token;
    std::vector<std::string> items;
    // Extract items using std::getline and '.' as the delimiter
    while (std::getline(iss, token, '.'))
    {
        if (!token.empty()) items.push_back(token);
    }
    assert(items.size() >= 3);
    auto name = items[2];
    auto directory = std::string("nvigi.plugin.") + name;
    if (items.size() > 3)
    {
        auto backend = items[3];
        directory += "." + backend;
    }
    return directory.c_str();
}

//! Internal framework API
//! 
//! Returns plugin ID for a given plugin name
//! 
PluginID getPluginIdFromName(const char* _name)
{
    PluginID id{};
    std::string name(_name);
    if (name == "nvigi.core.framework") return core::framework::kId;
    for (auto& item : ctx->modules)
    {
        auto& [path, internals] = item.second;
        if (name == path.filename().replace_extension().string())
        {
            id = item.first;
            break;
        }
    }
    return id;
}

//! Internal framework API
//! 
//! Returns path to dependencies, useful when plugins need to load shared libs dynamically
//! 
types::string getUTF8PathToDependencies()
{
    return ctx->utf8PathToDependencies.c_str();
}

// -----------------------------------------------------------------------

//! Check minimum specs for a give plugin
//! 
Result checkPluginMinSpec(nvigi::plugin::PluginInfo* info)
{
    if (info->minDriver > ctx->caps.driverVersion)
    {
        NVIGI_LOG_ERROR("Driver out of date, expected %s detected %s", extra::toStr(info->minDriver).c_str(), extra::toStr(ctx->caps.driverVersion).c_str());
        return nvigi::kResultDriverOutOfDate;
    }
#ifdef NVIGI_WINDOWS
    if (info->minOS > ctx->caps.osVersion)
    {
        NVIGI_LOG_ERROR("OS out of date, expected %s detected %s", extra::toStr(info->minOS).c_str(), extra::toStr(ctx->caps.osVersion).c_str());
        return nvigi::kResultOSOutOfDate;
    }
#endif
    if (info->pluginAPI > ctx->apiVersion)
    {
        NVIGI_LOG_ERROR("Framework out of date, version %s plugin %s", extra::toStr(ctx->apiVersion).c_str(), extra::toStr(info->pluginAPI).c_str());
        return nvigi::kResultInvalidState;
    }
    {
        // If requiredVendor is eNone, then we don't care if adapterCount is even nonzero
        // If requiredVendor is eAny, we DO care that a platform-compatible adapter is available.  We just don't care which vendor
        bool foundAdapter = info->requiredVendor == nvigi::VendorId::eNone;
        for (uint32_t i = 0; i < ctx->caps.adapterCount; i++)
        {
            auto& adapter = ctx->caps.adapters[i];
            //! Use this adapter if plugin:
            //! 
            //! * can use any adapter and does not care
            //! * requested this vendor and specific architecture
            //! 
            if (info->requiredVendor == nvigi::VendorId::eAny ||
                (adapter->vendor == info->requiredVendor && info->minGPUArch <= adapter->architecture))
            {
                foundAdapter = true;
                break;
            }
        }
        if (!foundAdapter)
        {
            NVIGI_LOG_WARN("Unable to find adapter supporting plugin, expected vendor 0x%x and GPU architecture %u", info->requiredVendor, info->minGPUArch);
            return nvigi::kResultNoSupportedHardwareFound;
        }
    }
    return kResultOk;
}

//! Helper to check if provided paths contain duplicated shared libraries
//! 
bool validateSharedLibraries(std::vector<fs::path>& visited)
{
    std::unordered_set<std::string> libraryNames;

    for (const auto& dir_path : visited)
    {
        for (const auto& entry : fs::directory_iterator(dir_path))
        {
            if (entry.is_regular_file()) {
                auto file_path = entry.path();
                std::string file_name = file_path.filename().string();

                // Check for .dll on Windows and .so on Linux
                bool is_shared_lib =
#if defined(_WIN32) || defined(_WIN64)
                    file_path.extension() == ".dll";
#else
                    file_path.extension() == ".so";
#endif
                if (is_shared_lib) {
                    if (libraryNames.find(file_name) != libraryNames.end()) {
                        // Duplicate found
                        NVIGI_LOG_ERROR("Found duplicated shared library '%s' - dependencies and plugins must NOT be duplicated", file_name.c_str());
                        return false;
                    }
                    else 
                    {
                        // Insert the library name into the set
                        libraryNames.insert(file_name);
#ifndef NVIGI_PRODUCTION
                        if (file_name.find("nvigi.") == std::string::npos && ctx->dependencies.find(file_name) == ctx->dependencies.end())
                        {
                            // This shared library was not loaded by any plugin, it could be that it is loaded dynamically but warn anyway if not in production
                            NVIGI_LOG_WARN("Found potentially unused shared library '%s'", file_name.c_str());
                        }
#endif
                    }
                }
            }
        }
    }
    return true;
}

//! Helper to unload any plugin
//! 
bool unloadPlugin(HMODULE hmod, const wchar_t* path)
{
    auto result = FreeLibrary(hmod);
#ifdef NVIGI_WINDOWS
    // On Windows result is BOOL
    if (!result)
    {
        auto lastError = GetLastError();
        NVIGI_LOG_ERROR("Failed to unload plugin '%S' - last error %s", path, std::system_category().message(GetLastError()).c_str());
        return false;
    }
    else
    {
        // Check to make sure plugin was actually freed
        wchar_t modulePath[nvigi::file::kMaxFilePath];
        if (GetModuleFileName(hmod, modulePath, nvigi::file::kMaxFilePath))
        {
            NVIGI_LOG_ERROR("Module is still loaded, path: '%S' - check for running threads or leaked DLL references, interfaces etc.", modulePath);
            return false;
        }
    }
#else
    // On Linux result is int, 0 success
    if (result)
    {
        NVIGI_LOG_ERROR("Failed to unload plugin '%S' - result %d - last error %s", path, result, std::system_category().message(errno).c_str());
        return false;
    }
#endif
    return true;
}

//! Loads plugins and returns the count
//! 
//! Note that plugins are immediatelly unloaded and started on explicit interface request
//! 
size_t enumeratePlugins(const char8_t* utf8Directory, bool validateDLLs, const nvigi::PluginID* requestedFeature = nullptr)
{
    size_t numPluginsFound = 0;
    auto utf16Directory = extra::utf8ToUtf16((const char*)utf8Directory);
    NVIGI_LOG_INFO("Scanning directory '%s' for plugins ...", utf8Directory);
#ifdef NVIGI_WINDOWS
    // Tell Win32 to load DLLs from these directories otherwise 3rd party dependencies won't be found
    // 
    // Always looking next to the plugin, system32 and in shared dependencies folder
    std::vector<std::wstring> utf16DependeciesDirectories = { utf16Directory};
    if (!ctx->utf8PathToDependencies.empty())
    {
        utf16DependeciesDirectories.push_back(extra::utf8ToUtf16(ctx->utf8PathToDependencies.c_str()));
    }
    file::ScopedDLLSearchPathChange changeDLLPath(utf16DependeciesDirectories);
#endif
    for (auto const& entry : fs::directory_iterator{ utf8Directory })
    {
        auto ext = entry.path().extension().u8string();
        auto name = entry.path().filename().u8string();
        auto dir = entry.path().parent_path().u8string();

        name = name.erase(name.find(ext));

#ifdef NVIGI_WINDOWS
        std::u8string dll = u8".dll";
#else
        std::u8string dll = u8".so";
#endif            
        if (ext == dll && name.find(u8"nvigi.plugin.") == 0)
        {
            unsigned long loadLibFlags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
            //! Prepare plugin specs to report back to the host
            auto tmp = new PluginSpec();
            ctx->pluginSpecs.push_back(tmp);
            PluginSpec& spec = *tmp;

            //! Set name early in case we fail for whatever reason
            auto charArray = new char[entry.path().filename().string().length() + 1];
            std::strcpy(charArray, entry.path().filename().string().c_str());
            spec.pluginName = charArray;

            // Make sure all dependencies came from the expected locations, if not this is an error
#ifdef NVIGI_WINDOWS
            std::map<std::string, fs::path> pluginDependencies{};
            if (validateDLLs)
            {
                if (!system::validateDLL(entry.path().wstring().c_str(), utf16DependeciesDirectories, pluginDependencies))
                {
                    NVIGI_LOG_WARN("Skipping plugin '%s' due to validation errors", name.c_str());
                    spec.status = kResultMissingDynamicLibraryDependency;
                    continue;
                }
#ifndef NVIGI_PRODUCTION
                ctx->dependencies.insert(pluginDependencies.begin(), pluginDependencies.end());
#endif
            }
#else
                // Silence warnings
            (void)validateDLLs;
            (void)loadLibFlags;
#endif
            //! ANSI C Win32 API does not support utf-8 hence using wchar_t
            //! 
            //! Also note that we must add flag to search for DLLs in user provided paths (see file::ScopedDLLSearchPathChange above)
            HMODULE hmod = LoadLibraryExW(entry.path().wstring().c_str(), NULL, loadLibFlags);
            if (!hmod)
            {
#ifdef NVIGI_WINDOWS
                NVIGI_LOG_ERROR("Failed to load plugin '%s' - error %s", name.c_str(), std::system_category().message(GetLastError()).c_str());
#else
                NVIGI_LOG_ERROR("Failed to load plugin '%s' - error %s", name.c_str(), dlerror());
#endif
                spec.status = kResultMissingDynamicLibraryDependency;
                continue;
            }            
            auto getFunc = (nvigi::plugin::PFun_PluginGetFunction*)GetProcAddress(hmod, "nvigiPluginGetFunction");
            if (!getFunc)
            {
                NVIGI_LOG_ERROR("Failed to map API for plugin %s", name.c_str());
                spec.status = kResultInvalidState;
                continue;
            }
            auto getInfo = (nvigi::plugin::PFun_PluginGetInfo*)getFunc("nvigiPluginGetInfo");
            nvigi::plugin::PluginInfo* info{};
            if (NVIGI_FAILED(error, getInfo(&nvigi::framework::ctx->framework, &info)))
            {
                NVIGI_LOG_ERROR("'getInfo' failed for plugin %s - error 0x%x", name.c_str(), error);
                spec.status = error;
                continue;
            }
            if (requestedFeature && info->id != *requestedFeature)
            {
                //! If specific plugin is requested and this is not it just skip it.
                //! 
                //! This is a NOP and below plugin just gets unloaded
            }
            else if (!isInterfaceSameOrNewerVersion(info))
            {
                NVIGI_LOG_WARN("Plugin '%s' is older than the framework - skipping ...", name.c_str());
                spec.status = kResultPluginOutOfDate;
            }
            else if (ctx->modules.find(info->id) != ctx->modules.end())
            {
                NVIGI_LOG_ERROR("Plugin '%s' has duplicated feature uid: %s crc24: 0x%x - skipping ...", name.c_str(), extra::guidToString(info->id.id).c_str(), info->id.crc24);
                spec.status = kResultDuplicatedPluginId;
            }
            else
            {
                ctx->modules[info->id] = { entry.path(), {} };
                
                NVIGI_LOG_INFO("Found plugin '%s':", name.c_str());
                NVIGI_LOG_INFO("# id: %s", extra::guidToString(info->id).c_str());
                NVIGI_LOG_INFO("# crc24: 0x%x", info->id.crc24);
                NVIGI_LOG_INFO("# description: '%s'", info->description.c_str());
                NVIGI_LOG_INFO("# version: %s", extra::toStr(info->pluginVersion).c_str());
                NVIGI_LOG_INFO("# build: %s", info->build.c_str());
                NVIGI_LOG_INFO("# author: '%s'", info->author.c_str());
                for (auto& interf : info->interfaces)
                {
                    NVIGI_LOG_INFO("# interface: {%s} v%u", extra::guidToString(interf.uid).c_str(), interf.version);
                }
#ifdef NVIGI_WINDOWS
                for (auto& [libName, libPath] : pluginDependencies)
                {
                    NVIGI_LOG_VERBOSE("# dependency '%s' found in '%S'", libName.c_str(), libPath.wstring().c_str());
                }
#endif
                //! Prepare info to report back if needed
                //! 
                //! NOTE: All dynamic allocations are deallocated on shutdown
                spec.id = info->id;
                spec.pluginAPI = info->pluginAPI;
                spec.pluginVersion = info->pluginVersion;
                spec.requiredOSVersion = info->minOS;
                spec.requiredAdapterVendor = info->requiredVendor;
                spec.requiredAdapterDriverVersion = info->minDriver;
                spec.requiredAdapterArchitecture = info->minGPUArch;
                spec.status = checkPluginMinSpec(info);
                spec.numSupportedInterfaces = info->interfaces.size();
                auto supportedInterfaces = new UID[spec.numSupportedInterfaces];
                for (size_t k = 0; k < spec.numSupportedInterfaces; k++)
                {
                    supportedInterfaces[k] = info->interfaces[k].uid;
                }
                spec.supportedInterfaces = supportedInterfaces;
#if !defined(NVIGI_PRODUCTION) && defined NVIGI_WINDOWS
                ctx->nameToId[std::string(name.begin(), name.end())] = info->id;
#endif
                numPluginsFound++;
            }
            unloadPlugin(hmod, entry.path().wstring().c_str());
        }
    }
    return numPluginsFound;
}

Result registerPlugin(nvigi::PluginID feature)
{
    if (ctx->modules.find(feature) == ctx->modules.end())
    {
        return nvigi::kResultMissingInterface;
    }
    auto& [path, internals] = ctx->modules[feature];

    if (!internals.hmod)
    {
        auto name = path.filename().string();
        name = name.substr(0, name.rfind("."));
        auto dir = path.parent_path().u8string();
        NVIGI_LOG_INFO("Attempting to register plugin [%s] located in '%s' ...", name.c_str(), dir.c_str());

#ifdef NVIGI_WINDOWS
        // Tell Win32 to load DLLs from these directories otherwise 3rd party dependencies won't be found
        // 
        // Always looking next to the plugin, system32 and in shared dependencies folder
        std::vector<std::wstring> utf16DependeciesDirectories = { path.parent_path().wstring()};
        if (!ctx->utf8PathToDependencies.empty())
        {
            utf16DependeciesDirectories.push_back(extra::utf8ToUtf16(ctx->utf8PathToDependencies.c_str()));
        }
        file::ScopedDLLSearchPathChange changeDLLPath(utf16DependeciesDirectories);

        // Validate DLL 
        std::map<std::string, fs::path> pluginDependencies{};
        if (!system::validateDLL(path.wstring().c_str(), utf16DependeciesDirectories, pluginDependencies))
        {
            NVIGI_LOG_WARN("Skipping plugin '%s' due to validation errors", name.c_str());
            return nvigi::kResultMissingDynamicLibraryDependency;
        }
#endif
        // Load our plugin and try to start it
        unsigned long loadLibFlags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
        //! ANSI C Win32 API does not support utf-8 hence using wchar_t
        //! 
        //! Also note that we must add flag to search for DLLs in user provided paths (see file::ScopedDLLSearchPathChange above)
        HMODULE hmod = LoadLibraryExW(path.wstring().c_str(), NULL, loadLibFlags);
        if (!hmod)
        {
            // This should be caught in enumerate pass really but just in case
#ifdef NVIGI_WINDOWS
            auto lastError = GetLastError();
#else
            (void)loadLibFlags; // silence warnings
            auto lastError = errno;
#endif
            NVIGI_LOG_ERROR("Failed to load plugin '%S' - last error %s", path.wstring().c_str(), std::system_category().message(lastError).c_str());
            return nvigi::kResultMissingDynamicLibraryDependency;
        }
        auto getFunc = (nvigi::plugin::PFun_PluginGetFunction*)GetProcAddress(hmod, "nvigiPluginGetFunction");
        if (!getFunc)
        {
            unloadPlugin(hmod, path.wstring().c_str());
            NVIGI_LOG_ERROR("Failed to map internal API for plugin %S", path.wstring().c_str());
            return nvigi::kResultInvalidState;
        }
        auto pluginRegister = (nvigi::plugin::PFun_PluginRegister*)getFunc("nvigiPluginRegister");
        auto pluginDeregister = (nvigi::plugin::PFun_PluginDeregister*)getFunc("nvigiPluginDeregister");
        auto getInfo = (nvigi::plugin::PFun_PluginGetInfo*)getFunc("nvigiPluginGetInfo");
        if (!pluginRegister || !pluginDeregister || !getInfo)
        {
            unloadPlugin(hmod, path.wstring().c_str());
            NVIGI_LOG_ERROR("Failed to map internal API for plugin %S", path.wstring().c_str());
            return nvigi::kResultInvalidState;
        }
        // Get plugin info
        nvigi::plugin::PluginInfo* info{};
        if (NVIGI_FAILED(error, getInfo(&nvigi::framework::ctx->framework, &info)))
        {
            NVIGI_LOG_ERROR("'getInfo' failed for plugin %S - error 0x%x", path.wstring().c_str(), error);
            return nvigi::kResultInvalidState;
        }
        //! Check min spec based on plugins' info
        //! 
        if (NVIGI_FAILED(error, checkPluginMinSpec(info)))
        {
            unloadPlugin(hmod, path.wstring().c_str());
            return error;
        }
        // Keep track of any existing interfaces and make sure plugin actually adds at least one
        size_t currentInterfaceCount = ctx->framework.getNumInterfaces(feature);
        if (NVIGI_FAILED(error, pluginRegister(&ctx->framework)))
        {
            unloadPlugin(hmod, path.wstring().c_str());
            NVIGI_LOG_ERROR("Failed to register plugin '%S' - error 0x%x", path.wstring().c_str(), error);
            return nvigi::kResultInvalidState;
        }
        if (currentInterfaceCount >= ctx->framework.getNumInterfaces(feature))
        {
            unloadPlugin(hmod, path.wstring().c_str());
            NVIGI_LOG_ERROR("Plugin '%S' did not add any interfaces and will be unloaded", path.wstring().c_str());
            return nvigi::kResultInvalidState;
        }
        internals.hmod = hmod;
        internals.pluginDeregister = pluginDeregister;
        ctx->modules[feature] = { path, internals };
    }
    return kResultOk;
}

}
}

using namespace nvigi::framework;

//! Core API
//! 
//! Initialize framework, enumerate plugins
//! 
nvigi::Result nvigiInitImpl(const nvigi::Preferences& pref, nvigi::PluginAndSystemInformation** pluginInfo, uint64_t sdkVersion)
{
    if (ctx) return nvigi::kResultInvalidState;

    ctx = new FrameworkContext;

    // Always validate DLLs when enumerating plugins (but NOT when registering them for use later on)
    bool validateDLLs = true;

    nvigi::VendorId forceAdapterId = nvigi::VendorId::eAny;
    uint32_t forceArchitecture = 0;

    // Setup logging
    auto log = nvigi::log::getInterface();
    log->enableConsole(pref.showConsole);
    log->setLogLevel(pref.logLevel);
    log->setLogCallback((void*)pref.logMessageCallback);
    log->setLogName("nvigi-log.txt");
    log->setLogPath(pref.utf8PathToLogsAndData); // this can fail so set it last in case we need to print to console or report back via callback

    std::string buildConfig("production");
#ifdef NVIGI_DEBUG
    buildConfig = "debug";
#elif NVIGI_RELEASE
    buildConfig = "release";
#endif
    NVIGI_LOG_INFO("Starting 'nvigi.core.framework':");
    NVIGI_LOG_INFO("# time-stamp: %s", __TIMESTAMP__);
    NVIGI_LOG_INFO("# version: %s [%s]", nvigi::extra::toStr(nvigi::Version(NVIGI_CORESDK_VERSION_MAJOR, NVIGI_CORESDK_VERSION_MINOR, NVIGI_CORESDK_VERSION_PATCH)).c_str(), buildConfig.c_str());
    NVIGI_LOG_INFO("# API version: %s", nvigi::extra::toStr(nvigi::Version(NVIGI_CORESDK_API_VERSION_MAJOR, NVIGI_CORESDK_API_VERSION_MINOR, NVIGI_CORESDK_API_VERSION_PATCH)).c_str());
    NVIGI_LOG_INFO("# build: %s", GIT_BRANCH_AND_LAST_COMMIT);
    NVIGI_LOG_INFO("# author: NVIDIA");
    NVIGI_LOG_INFO("# host SDK: %s", nvigi::extra::toStr(ctx->hostSDKVersion).c_str());

    nvigi::system::setPreferenceFlags(pref.flags);

#ifdef NVIGI_WINDOWS
    //! If process is running with elevated privileges we downgrade them for security reasons
    nvigi::system::ScopedDowngradePrivileges guardPrivileges;

    std::string miniDumpDirectory;
    if (nvigi::extra::getEnvVar("NVIGI_OVERRIDE_DUMP_PATH", miniDumpDirectory))
    {
        nvigi::exception::getInterface()->setMiniDumpLocation(miniDumpDirectory.c_str());
    }
#endif

    // SDK version is 64bit split in four 16bit values
    // 
    // major | minor | patch | magic
    //
    if ((sdkVersion & nvigi::kSDKVersionMagic) == nvigi::kSDKVersionMagic)
    {
        ctx->hostSDKVersion = { (uint32_t)((sdkVersion >> 48) & 0xffff), (uint32_t)((sdkVersion >> 32) & 0xffff), (uint32_t)((sdkVersion >> 16) & 0xffff) };
    }
    else
    {
        NVIGI_LOG_ERROR("Invalid SDK version provided");
        return nvigi::kResultInvalidParameter;
    }

    // Collect dependencies location from host if provided
    //
    // If not provided we assume that all dependencies are next to the plugin(s).
    // 
    // NOTE: This is allowed as long as shared dependencies are not duplicated.
    if (pref.utf8PathToDependencies)
    {
        // On Win this can alter the path to handle long paths (over MAX_PATH) and convert symlinks, relative paths to absolute etc.
        if (!nvigi::file::getOSValidDirectoryPath(pref.utf8PathToDependencies, ctx->utf8PathToDependencies))
        {
            // NOTE: always log paths as unicode, fs::path constructor does NOT throw exceptions
            NVIGI_LOG_ERROR("Provided path '%S' is not valid", fs::path(pref.utf8PathToDependencies).wstring().c_str());
            return nvigi::kResultInvalidParameter;
        }
        // At this point 'ctx->utf8PathToDependencies' is absolute, normalized and "long" if over MAX_PATH on Win11 and it points to a valid directory
    }

    // Override various settings via JSON in non-production builds
#ifndef NVIGI_PRODUCTION
    std::vector<std::wstring> jsonLocations = { nvigi::file::getExecutablePath(), nvigi::file::getModulePath(), nvigi::file::getCurrentDirectoryPath() };
    for (auto& path : jsonLocations)
    {
        std::wstring interposerJSONFile = path + L"/nvigi.core.framework.json";
        if (nvigi::file::exists(interposerJSONFile.c_str()))
        {
            auto jsonText = nvigi::file::read(interposerJSONFile.c_str());
            if (!jsonText.empty()) try
            {
                // safety null in case the JSON string is not null-terminated (found by AppVerif)
                jsonText.push_back(0);
                std::istringstream stream((const char*)jsonText.data());
                json config;
                stream >> config;

                log->enableConsole(nvigi::extra::getJSONValue(config, "showConsole", pref.showConsole));
                log->setLogLevel(nvigi::extra::getJSONValue(config, "logLevel", pref.logLevel));
                std::string utf8PathToLogsAndData = pref.utf8PathToLogsAndData ? pref.utf8PathToLogsAndData : "";
                log->setLogPath(nvigi::extra::getJSONValue(config, "logPath", utf8PathToLogsAndData).c_str());

                NVIGI_LOG_INFO("Overriding settings with parameters from '%S'", interposerJSONFile.c_str());

#ifdef NVIGI_WINDOWS
                bool waitForDebugger = false;
                waitForDebugger = nvigi::extra::getJSONValue(config, "waitForDebugger", waitForDebugger);
                if (waitForDebugger)
                {
                    NVIGI_LOG_INFO("Waiting for a debugger to attach ...");
                    while (!IsDebuggerPresent())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
                std::string miniDumpDirectory;
                miniDumpDirectory = nvigi::extra::getJSONValue(config, "miniDumpDirectory", miniDumpDirectory);
                if (!miniDumpDirectory.empty())
                {
                    nvigi::exception::getInterface()->setMiniDumpLocation(miniDumpDirectory.c_str());
                }
#endif
                ctx->registerPlugins = nvigi::extra::getJSONValue(config, "registerPlugins", ctx->registerPlugins);
                ctx->utf8PathToPlugins = nvigi::extra::getJSONValue(config, "pathToPlugins", ctx->utf8PathToPlugins);
                ctx->utf8PathToDependencies = nvigi::extra::getJSONValue(config, "pathToDependencies", ctx->utf8PathToDependencies);

                // Optional so can be invalid if not provided hence not checking result
                nvigi::file::getOSValidDirectoryPath(ctx->utf8PathToPlugins.c_str(), ctx->utf8PathToPlugins);
                nvigi::file::getOSValidDirectoryPath(ctx->utf8PathToDependencies.c_str(), ctx->utf8PathToDependencies);

                validateDLLs = nvigi::extra::getJSONValue(config, "validateDLLs", validateDLLs);
                std::string forceAdapterStr = nvigi::extra::getJSONValue(config, "forceAdapter", std::string("0")); // "0" == eAny
                std::string forceArchitectureStr = nvigi::extra::getJSONValue(config, "forceArchitecture", std::string("0"));

                forceAdapterId = (nvigi::VendorId)std::stoi(forceAdapterStr, nullptr, 16);
                forceArchitecture = std::stoi(forceArchitectureStr, nullptr, 16);
            }
            catch (std::exception& e)
            {
                NVIGI_LOG_ERROR("'nvigi.core.framework.json' exception %s", e.what());
            }
            break;
        }
    }
#endif

    // Share internal interface for logging, memory management and exception handling
    addInterface(nvigi::core::framework::kId, log, nvigi::framework::InterfaceFlagNotRefCounted);
    addInterface(nvigi::core::framework::kId, nvigi::memory::getInterface(), nvigi::framework::InterfaceFlagNotRefCounted);
    addInterface(nvigi::core::framework::kId, nvigi::exception::getInterface(), nvigi::framework::InterfaceFlagNotRefCounted);
    addInterface(nvigi::core::framework::kId, nvigi::system::getInterface(), nvigi::framework::InterfaceFlagNotRefCounted);

    // Setup internal framework interface - shared via core API with each plugin
    ctx->framework.addInterface = addInterface;
    ctx->framework.getInterface = getInterface;
    ctx->framework.releaseInterface = releaseInterface;
    ctx->framework.getNumInterfaces = getNumInterfaces;
    ctx->framework.getModelDirectoryForPlugin = getModelDirectoryForPlugin;
    ctx->framework.getPluginIdFromName = getPluginIdFromName;
    ctx->framework.getUTF8PathToDependencies = getUTF8PathToDependencies;

    // Get system info
    nvigi::system::getSystemCaps(forceAdapterId, forceArchitecture, &ctx->caps);

    // Get OS version and update timer resolution
    nvigi::system::getOSVersionAndUpdateTimerResolution(&ctx->caps);

    if(!(pref.flags & nvigi::PreferenceFlags::eDisableCPUTimerResolutionChange))
    {
        // Update timer resolution
        nvigi::system::setTimerResolution();
    }    

    // Check if JSON was used to override path provided by the host
#ifndef NVIGI_PRODUCTION
    if (!ctx->utf8PathToPlugins.empty())
    {
        if (!fs::is_directory(ctx->utf8PathToPlugins))
        {
            // This can happen in ReShade, if we use relative path but app sets working directory to something else SDK will fail to find plugins
            NVIGI_LOG_WARN("'%s' is NOT a valid directory, trying as relative path to the executable ...", ctx->utf8PathToPlugins.c_str());
            fs::path pathToPlugins(nvigi::file::getExecutablePath() + nvigi::extra::utf8ToUtf16(ctx->utf8PathToPlugins.c_str()));
            if (fs::is_directory(pathToPlugins))
            {
                ctx->utf8PathToPlugins = pathToPlugins.string();
            }
            else
            {
                NVIGI_LOG_ERROR("'%s' also NOT a valid directory", pathToPlugins.c_str());
            }
        }
        enumeratePlugins((const char8_t*)ctx->utf8PathToPlugins.c_str(), validateDLLs);
    }
    else
#endif
    {
        // Track visited directories
        std::vector<fs::path> visited;        
        // Enumerate plugins and get information from them, allow plugins in different directories
        for (uint32_t i = 0; i < pref.numPathsToPlugins; i++) try
        {
            if (!pref.utf8PathsToPlugins[i])
            {
                NVIGI_LOG_ERROR("Path to plugins at index %u is null", i);
                return nvigi::kResultInvalidParameter;
            }
            std::string utf8Path;
            // On Win this can alter the path to handle long paths (over MAX_PATH) and convert symlinks, relative paths to absolute etc.
            if (!nvigi::file::getOSValidDirectoryPath(pref.utf8PathsToPlugins[i], utf8Path))
            {
                // NOTE: always log paths as unicode, fs::path constructor does NOT throw exceptions
                NVIGI_LOG_ERROR("Provided path '%S' is not valid", fs::path(pref.utf8PathsToPlugins[i]).wstring().c_str());
                return nvigi::kResultInvalidParameter;
            }
            // At this point path to plugins is absolute, normalized and "long" if over MAX_PATH on Win11 and it points to a valid directory
            auto path = fs::path(utf8Path);
            // We don't want to visit same path multiple times
            if (std::find(visited.begin(), visited.end(), path) != visited.end())
            {
                NVIGI_LOG_WARN("Duplicated path to plugins provided, ignoring '%S'", path.wstring().c_str());
                continue;
            }
            visited.push_back(path);
            enumeratePlugins(path.u8string().c_str(), validateDLLs);
        }
        catch (std::exception& e)
        {
            NVIGI_LOG_ERROR("std::filesystem exception %s", e.what());
            return nvigi::kResultInvalidState;
        }
        if (!ctx->utf8PathToDependencies.empty())
        {
            // We don't expect any plugins to be in the dependencies directory so let's check that also
            if (std::find(visited.begin(), visited.end(), ctx->utf8PathToDependencies) == visited.end())
            {
                visited.push_back(ctx->utf8PathToDependencies);
            }
        }
        // Make sure provided paths from the host side do not contain duplicated shared libraries
        if (!validateSharedLibraries(visited))
        {
            return nvigi::kResultInvalidState;
        }
    }


    if (ctx->modules.empty())
    {
        NVIGI_LOG_ERROR("Failed to find any plugins. Please make sure to provide at least one valid path to plugins.");
        return nvigi::kResultNoPluginsFound;
    }

     //! Check if requested and report back info to the host
    if (pluginInfo)
    {
        *pluginInfo = &ctx->pluginSysInfo;
        auto info = *pluginInfo;
        for (uint32_t i = 0; i < ctx->caps.adapterCount; i++)
        {
            // Deleted on shutdown
            auto adapter = new nvigi::AdapterSpec;
            adapter->id = ctx->caps.adapters[i]->id;
            adapter->driverVersion = ctx->caps.driverVersion;
            adapter->vendor = ctx->caps.adapters[i]->vendor;
            adapter->dedicatedMemoryInMB = ctx->caps.adapters[i]->dedicatedMemoryInMB;
            adapter->architecture = ctx->caps.adapters[i]->architecture;
            ctx->adapterSpecs.push_back(adapter);
        }
        info->osVersion = ctx->caps.osVersion;
        info->numDetectedPlugins = ctx->pluginSpecs.size();
        info->detectedPlugins = ctx->pluginSpecs.data();
        info->numDetectedAdapters = ctx->caps.adapterCount;
        info->detectedAdapters = ctx->adapterSpecs.data();
        info->flags = ctx->caps.flags;
    }

#ifndef NVIGI_PRODUCTION
    for (auto& name : ctx->registerPlugins)
    {
        if (ctx->nameToId.find(name) == ctx->nameToId.end())
        {
            NVIGI_LOG_WARN("Plugin [%s] was not enumerated and cannot be registered", name.c_str());
            continue;
        }
        auto id = ctx->nameToId[name];
        NVIGI_CHECK(registerPlugin(id));
    }
#endif
    return nvigi::kResultOk;
}

nvigi::Result nvigiShutdownImpl()
{
    if (!ctx) return nvigi::kResultInvalidState;

#ifdef NVIGI_WINDOWS
    //! If process is running with elevated privileges we downgrade them for security reasons
    nvigi::system::ScopedDowngradePrivileges guardPrivileges;
#endif

    // Release adapters
    nvigi::system::cleanup(&ctx->caps);

    nvigi::Result result = nvigi::kResultOk;
    // Release all plugins
    for (auto& item : ctx->modules)
    {
        // Check for unreleased interfaces
        auto& list = ctx->interfaces[item.first];
        for (auto it = list.begin(); it != list.end(); it++)
        {
            auto& [refCount, i, flags] = (*it);
            bool counted = !(flags & nvigi::framework::InterfaceFlagNotRefCounted);
            if (counted && refCount > 0)
            {
                NVIGI_LOG_ERROR("Plugin [%s] leaked interface '%s'", getPluginName(item.first).c_str(), nvigi::extra::guidToString(i->type).c_str());
                result = nvigi::kResultInvalidState;
            }
        }

        auto& [path, internals] = item.second;
        if (internals.hmod)
        {
            NVIGI_LOG_INFO("Shutting down plugin '%S'", path.wstring().c_str());
            NVIGI_VALIDATE(internals.pluginDeregister());
            if (!unloadPlugin(internals.hmod, path.wstring().c_str()))
            {
                result = nvigi::kResultInvalidState;
            }
        }
    }
    ctx->modules.clear();

    // Delete plugin and adapter public facing info
    for (auto& item : ctx->pluginSpecs)
    {
        delete [] item->pluginName;
        delete [] item->supportedInterfaces;
        delete item;
    }
    for (auto& item : ctx->adapterSpecs)
    {
        delete item;
    }

    nvigi::log::destroyInterface();
    nvigi::exception::destroyInterface();

    delete ctx;
    ctx = nullptr;

#if defined NVIGI_WINDOWNS && defined  NVIGI_VALIDATE_MEMORY
    assert(nvigi::memory::getInterface()->getNumAllocations() == 0);
#endif
    return result;
}

nvigi::Result nvigiLoadInterfaceImpl(nvigi::PluginID feature, const nvigi::UID& type, uint32_t /*version*/, void** _interface, const char* utf8PathToPlugin)
{
    if (!_interface) return nvigi::kResultInvalidParameter;
    if (!ctx) return nvigi::kResultInvalidState;

#ifdef NVIGI_WINDOWS
    //! If process is running with elevated privileges we downgrade them for security reasons
    nvigi::system::ScopedDowngradePrivileges guardPrivileges;
#endif

    auto& list = ctx->interfaces[feature];

    if (list.empty())
    {
        // No interfaces for this feature, check if we never saw this plugin before and user provided new path to find it
        if (ctx->modules.find(feature) == ctx->modules.end() && utf8PathToPlugin) try
        {
            std::string utf8Path;
            // On Win this can alter the path to handle long paths (over MAX_PATH) and convert symlinks, relative paths to absolute etc.
            if (!nvigi::file::getOSValidDirectoryPath(utf8PathToPlugin, utf8Path))
            {
                // NOTE: always log paths as unicode, fs::path constructor does NOT throw exceptions
                NVIGI_LOG_ERROR("Provided path '%S' is not valid", fs::path(utf8PathToPlugin).wstring().c_str());
                return nvigi::kResultInvalidParameter;
            }
            // At this point path is absolute, normalized and "long" if over MAX_PATH on Win11 and it points to a valid directory
            auto path = fs::path(utf8Path);
            if (!enumeratePlugins(path.u8string().c_str(), true, &feature))
            {
                NVIGI_LOG_WARN("No new plugins found or loaded from the provided path '%S' when requesting interface {%s}", path.wstring().c_str(), nvigi::extra::guidToString(type).c_str());
            }
            else
            {
                // Pointer to this structure is returned to the host when nvigiInit is called so update it
                ctx->pluginSysInfo.numDetectedPlugins = ctx->pluginSpecs.size();
                ctx->pluginSysInfo.detectedPlugins = ctx->pluginSpecs.data();
            }
        }
        catch (std::exception& e)
        {
            // std::filesystem can throw exceptions on invalid inputs
            NVIGI_LOG_ERROR("Exception '%s'", e.what());
            return nvigi::kResultInvalidParameter;
        }

        NVIGI_CHECK(registerPlugin(feature));
    }

    for (auto& [refCount, i, flags] : list)
    {
        //! Not checking version here, it is OK to provide older interface
        //!
        //! Interface consumer must check the version if accessing v2+ members
        if (i->type == type)
        {
            if (!(flags & nvigi::framework::InterfaceFlagNotRefCounted))
            {
                refCount++;
                NVIGI_LOG_VERBOSE("Tracking interface '%s' (refCount %d) from plugin [%s]", nvigi::extra::guidToString(i->type).c_str(), refCount, getPluginName(feature).c_str());
            }
            //! No need to log untracked internal interface usage like IMemory, ISystem, ILog etc. - it clogs the logs and can be confusing
            *_interface = i;
            break;
        }
    }

    return *_interface ? nvigi::kResultOk : nvigi::kResultMissingInterface;
}

nvigi::Result nvigiUnloadInterfaceImpl(nvigi::PluginID feature, const nvigi::UID& type)
{
    if (!ctx) return nvigi::kResultInvalidState;

#ifdef NVIGI_WINDOWS
    //! If process is running with elevated privileges we downgrade them for security reasons
    nvigi::system::ScopedDowngradePrivileges guardPrivileges;
#endif

    auto& list = ctx->interfaces[feature];

    // We start with the assumption that we will NOT find this interface
    auto result = nvigi::kResultInvalidParameter;

    bool deletedInterface = false;
    bool remainingInterfaces = false;
    for (auto it = list.begin(); it != list.end(); it++)
    {
        auto& [refCount, i, flags] = (*it);
        bool counted = !(flags & nvigi::framework::InterfaceFlagNotRefCounted);
        if (type == i->type)
        {
            // We found our interface so success!
            result = nvigi::kResultOk;
            if (counted)
            {
                refCount--;
                assert(refCount >= 0);
                if (refCount <= 0)
                {
                    NVIGI_LOG_VERBOSE("[%s] unloading interface '%s' (with refCount %d)", getPluginName(feature).c_str(), nvigi::extra::guidToString(i->type).c_str(), refCount);
                    deletedInterface = true;
                }
                else
                {
                    NVIGI_LOG_VERBOSE("[%s] removed ref to interface '%s' (with refCount %d)", getPluginName(feature).c_str(), nvigi::extra::guidToString(i->type).c_str(), refCount);
                }
            }
        }
        else if (!counted || (refCount > 0))
        {
            // Other interfaces still used, we cannot unload the entire plugin
            remainingInterfaces = true;
        }
    }

    if (deletedInterface && !remainingInterfaces)
    {
        if (ctx->modules.find(feature) == ctx->modules.end())
        {
            return nvigi::kResultMissingInterface;
        }
        auto& [path, internals] = ctx->modules[feature];
        if (internals.hmod)
        {
            NVIGI_LOG_INFO("Shutting down plugin '%S'", path.wstring().c_str());
            NVIGI_VALIDATE(internals.pluginDeregister());
            if (!unloadPlugin(internals.hmod, path.wstring().c_str()))
            {
                // unloadPlugin logs the appropriate error so no need to do anything here other than return an error
                result = nvigi::kResultInvalidState;
            }
            internals.hmod = nullptr;
            internals.pluginDeregister = nullptr;
        }
        ctx->interfaces.erase(feature);
    }
    else if (result != nvigi::kResultOk)
    {
        NVIGI_LOG_ERROR("Failed to find and unload interface {%s} for plugin [%s]", nvigi::extra::guidToString(type).c_str(), getPluginName(feature).c_str());
    }
    return result;
}

nvigi::Result nvigiInit(const nvigi::Preferences& pref, nvigi::PluginAndSystemInformation** pluginInfo, uint64_t sdkVersion)
{
    NVIGI_CATCH_EXCEPTION(nvigiInitImpl(pref, pluginInfo, sdkVersion));
}

nvigi::Result nvigiShutdown()
{
    NVIGI_CATCH_EXCEPTION(nvigiShutdownImpl());
}

nvigi::Result nvigiLoadInterface(nvigi::PluginID feature, const nvigi::UID& type, uint32_t version, void** _interface, const char* utf8PathToPlugin)
{
    NVIGI_CATCH_EXCEPTION(nvigiLoadInterfaceImpl(feature, type, version, _interface, utf8PathToPlugin));
}

nvigi::Result nvigiUnloadInterface(nvigi::PluginID feature, void* _interface)
{
    if (_interface == nullptr)
        return nvigi::kResultMissingInterface;
    NVIGI_CATCH_EXCEPTION(nvigiUnloadInterfaceImpl(feature, ((nvigi::BaseStructure*)_interface)->type));
}
