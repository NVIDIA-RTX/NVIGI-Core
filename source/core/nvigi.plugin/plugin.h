// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.system/system.h"
#include "source/core/nvigi.types/types.h"
#include "source/core/nvigi.framework/framework.h"
#include "external/json/source/nlohmann/json.hpp"
using json = nlohmann::json;

#ifdef NVIGI_WINDOWS
#define NVIGI_EXPORT extern "C" __declspec(dllexport)
NVIGI_EXPORT BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID);
#else
#define NVIGI_EXPORT extern "C" __attribute__((visibility("default")))
void so_attach(void);
void so_detach(void);
#endif

namespace nvigi
{

namespace log
{
struct ILog;
ILog* getInterface();
}

namespace memory
{
struct IMemoryManager;
IMemoryManager* getInterface();
}

namespace system
{
struct ISystem;
ISystem* getInterface();
}

namespace plugin
{

bool internalPluginSetup(nvigi::framework::IFramework* framework);

//! Interface 'InterfaceInfo'
//!
//! {1D599C9D-CDFC-4DB9-97A5-80F817211C1C}
struct alignas(8) InterfaceInfo
{
    InterfaceInfo() { };
    InterfaceInfo(UID _uid, uint32_t _version) : uid(_uid), version(_version) {};
    NVIGI_UID(UID({ 0x1d599c9d, 0xcdfc, 0x4db9,{0x97, 0xa5, 0x80, 0xf8, 0x17, 0x21, 0x1c, 0x1c} }), kStructVersion1)

    UID uid;
    uint32_t version;

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(InterfaceInfo)

template<typename T>
InterfaceInfo getInterfaceInfo()
{
    T i;
    return { i.getType(),i.getVersion() };
}

// {53FAF63E-8B92-477C-94B5-B60B4B9DBF48}
struct alignas(8) PluginInfo {
    PluginInfo() {}; 
    NVIGI_UID(UID({ 0x53faf63e, 0x8b92, 0x477c,{ 0x94, 0xb5, 0xb6, 0xb, 0x4b, 0x9d, 0xbf, 0x48 } }), kStructVersion1)
    PluginInfo(const PluginInfo& other)
    {
        id = other.id;
        pluginVersion = other.pluginVersion;
        pluginAPI = other.pluginAPI;
        minOS = other.minOS;
        minDriver = other.minDriver;
        minGPUArch = other.minGPUArch;
        description = other.description;
        author = other.author;
        build = other.build;
        interfaces = other.interfaces;
    }

    PluginID id;
    Version pluginVersion;
    Version pluginAPI;
    Version minOS{};
    Version minDriver{};
    uint32_t minGPUArch{};
    VendorId requiredVendor{};
    types::string description{};
    types::string author{};
    types::string build{};
    types::vector<InterfaceInfo> interfaces{};
};

NVIGI_VALIDATE_STRUCT(PluginInfo)

// Core API, each plugin must implement these
using PFun_PluginGetInfo = Result(nvigi::framework::IFramework* framework, nvigi::plugin::PluginInfo** info);
using PFun_PluginRegister = Result(nvigi::framework::IFramework* framework);
using PFun_PluginDeregister = Result(void);
using PFun_PluginGetFunction = void*(const char* name);

#ifdef NVIGI_WINDOWS

#define NVIGI_ENTRY_POINT friend BOOL APIENTRY ::DllMain(HMODULE hModule, DWORD fdwReason, LPVOID);

#define NVIGI_ENTRY_POINT_BODY(N,V1,V2,PLUGIN_NAMESPACE, PLUGIN_CTX)                                        \
NVIGI_EXPORT BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)                                \
{                                                                                                          \
    switch (fdwReason)                                                                                     \
    {                                                                                                      \
        case DLL_PROCESS_ATTACH:                                                                           \
            nvigi::plugin::s_ctx = new nvigi::plugin::Context(N, nvigi::V1, nvigi::V2);                        \
            nvigi::PLUGIN_NAMESPACE::s_ctx = new nvigi::PLUGIN_NAMESPACE::PLUGIN_CTX();                      \
            break;                                                                                         \
        case DLL_THREAD_ATTACH:                                                                            \
            break;                                                                                         \
        case DLL_THREAD_DETACH:                                                                            \
            break;                                                                                         \
        case DLL_PROCESS_DETACH:                                                                           \
            delete nvigi::plugin::s_ctx;                                                                    \
            delete nvigi::PLUGIN_NAMESPACE::s_ctx;                                                          \
            nvigi::plugin::s_ctx = {};                                                                      \
            nvigi::PLUGIN_NAMESPACE::s_ctx = {};                                                            \
            break;                                                                                         \
    }                                                                                                      \
    return TRUE;                                                                                           \
}                                                                                                          \

#else

#define NVIGI_ENTRY_POINT                                                                                     \
friend void ::so_attach(void);                                                                             \
friend void ::so_detach(void);                                                                             \

#define NVIGI_ENTRY_POINT_BODY(N,V1,V2,PLUGIN_NAMESPACE, PLUGIN_CTX)                                          \
__attribute__((constructor)) void so_attach(void) {                                                        \
    nvigi::plugin::s_ctx = new nvigi::plugin::Context(N, nvigi::V1, nvigi::V2);                                 \
    nvigi::PLUGIN_NAMESPACE::s_ctx = new nvigi::PLUGIN_NAMESPACE::PLUGIN_CTX();                                  \
}                                                                                                          \
__attribute__((destructor)) void so_detach(void) {                                                         \
    delete nvigi::plugin::s_ctx;                                                                                 \
    delete nvigi::PLUGIN_NAMESPACE::s_ctx;                                                                    \
    nvigi::plugin::s_ctx = {};                                                                                   \
    nvigi::PLUGIN_NAMESPACE::s_ctx = {};                                                                      \
}                                                                                                          \

#endif

//! Plugin specific context, with default constructor and destructor
//! 
//! NOTE: Instance of this context is valid for the entire life-cycle of a plugin since it cannot
//! be destroyed anywhere else other than in DLLMain when plugin is detached from the process.
//! 
#define NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(NAME)                                  \
protected:                                                                      \
    NAME() {onCreateContext();};                                                \
    /* Called on exit from DLL */                                               \
    ~NAME() {onDestroyContext();};                                              \
    NVIGI_ENTRY_POINT                                                              \
public:                                                                         \
    NAME(const NAME& rhs) = delete;

//! Plugin specific context, same as above except constructor is not auto-defined
//! 
#define NVIGI_PLUGIN_CONTEXT_DESTROY_ONLY(NAME)                                    \
protected:                                                                      \
    /* Called on exit from DLL */                                               \
    ~NAME() {onDestroyContext();};                                              \
    NVIGI_ENTRY_POINT                                                              \
public:                                                                         \
    NAME(const NAME& rhs) = delete;

//! Generic context, same across all plugins
//! 
//! Contains basic information like versions, name, JSON configurations etc.
//! 
class Context
{
    Context(
        const std::string& _pluginName,
        Version _pluginVersion,
        Version _apiVersion)
    {
        pluginName = _pluginName;
        info.pluginVersion = _pluginVersion;
        info.pluginAPI = _apiVersion;
    };

    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(Context)

    void onCreateContext() {};
    void onDestroyContext() 
    {
        delete pluginConfig;
    };

    std::string pluginName{};
    PluginInfo info{};
    json* pluginConfig{};
    framework::IFramework* framework{};
};

Context *getContext();

#define NVIGI_PLUGIN_CONTEXT_DEFINE(PLUGIN_NAMESPACE, PLUGIN_CTX)                             \
namespace PLUGIN_NAMESPACE                                                                   \
{                                                                                            \
    /* Created on DLL attached and destroyed on DLL detach from process */                   \
    static PLUGIN_CTX* s_ctx{};                                                              \
    PLUGIN_CTX* getContext() { return s_ctx; }                                               \
}

//! Core definitions, each plugin must use this define and specify versions
//! 
//! NOTE: This macro must be placed within 'namespace nvigi'
//! 
#define NVIGI_PLUGIN_DEFINE(N,V1,V2,PLUGIN_NAMESPACE, PLUGIN_CTX)                                           \
namespace plugin                                                                                           \
{                                                                                                          \
    /* Created on DLL attached and destroyed on DLL detach from process */                                 \
    static Context* s_ctx{};                                                                               \
    Context *getContext() { return s_ctx; }                                                                \
}                                                                                                          \
                                                                                                           \
NVIGI_PLUGIN_CONTEXT_DEFINE(PLUGIN_NAMESPACE, PLUGIN_CTX)                                                   \
                                                                                                           \
                                                                                                           \
}  /* namespace nvigi */                                                                                    \
                                                                                                           \
/* Always in global namespace */                                                                           \
NVIGI_ENTRY_POINT_BODY(N,V1,V2,PLUGIN_NAMESPACE, PLUGIN_CTX)                                                \
                                                                                                           \
namespace nvigi {

} // namespace api

namespace plugin
{

#define NVIGI_EXPORT_FUNCTION(fun)\
if (!strcmp(functionName, #fun))\
{\
    return (void*)fun;\
}

#ifdef NVIGI_WINDOWS
 #define NVIGI_EXPORT_OTA                                       \
if (api::getContext()->getPluginFunction)                      \
{                                                              \
    return api::getContext()->getPluginFunction(functionName); \
}
#else
#define NVIGI_EXPORT_OTA
#endif

}
}
