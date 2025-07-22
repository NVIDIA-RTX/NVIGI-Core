// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.api/nvigi_security.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.plugin/plugin.h"
#include "source/plugins/nvigi.hwi/d3d12/versions.h"
#include "source/plugins/nvigi.hwi/common/nvigi_hwi_common.h"
#include "_artifacts/gitVersion.h"

#include "d3d_scheduler_settings.h"
#include "nvigi_hwi_d3d12.h"

#include "dxgi.h"
#include "external/nvapi/nvapi.h"

#include <unordered_set>

namespace nvigi
{
namespace hwiD3D12
{
struct D3D12Context
{
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(D3D12Context);

    void onCreateContext()
    {
    };

    void onDestroyContext()
    {
    };

    IHWID3D12 api{};

    IHWICommon* hwiCommon{};
    CigSchedulerSettingsAPI sched;
    HMODULE cigHelper{};

    std::unordered_set<ID3D12Device*> initializedDevices;
};
};

//! Define our plugin
//! 
//! IMPORTANT: Make sure to place this macro right after the context declaration and always within the 'nvigi' namespace ONLY.
NVIGI_PLUGIN_DEFINE("nvigi.plugin.hwi.d3d12", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(API_MAJOR, API_MINOR, API_PATCH), hwiD3D12, D3D12Context)

static nvigi::Result d3d12ApplyGlobalGpuInferenceSchedulingModeToThread(ID3D12Device* device)
{
    auto& ctx = (*hwiD3D12::getContext());

    uint32_t schedulingMode;
    ctx.hwiCommon->GetGpuInferenceSchedulingMode(&schedulingMode);

    // Check that the public and private enum types are equal
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_FOREGROUND) ==
        int(SchedulingMode::kPrioritizeCompute));
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_BACKGROUND) ==
        int(SchedulingMode::kBalance));
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_BACKGROUND_WITH_THROTTLE) ==
        int(SchedulingMode::kPrioritizeGraphics));

    int err = ctx.sched.ThreadSetD3DWorkloadType(device, CigWorkloadType(schedulingMode));

    nvigi::Result retval = kResultOk;
    if (err == NvAPI_Status::NVAPI_NO_IMPLEMENTATION) retval = kResultDriverOutOfDate;

    return retval;
}

static nvigi::Result d3d12ApplyGlobalGpuInferenceSchedulingModeToCommandList(ID3D12GraphicsCommandList* commandList)
{
    auto& ctx = (*hwiD3D12::getContext());

    uint32_t schedulingMode;
    ctx.hwiCommon->GetGpuInferenceSchedulingMode(&schedulingMode);

    // Check that the public and private enum types are equal
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_FOREGROUND) ==
        int(SchedulingMode::kPrioritizeCompute));
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_BACKGROUND) ==
        int(SchedulingMode::kBalance));
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_BACKGROUND_WITH_THROTTLE) ==
        int(SchedulingMode::kPrioritizeGraphics));

    int err = ctx.sched.CommandListSetD3DWorkloadType(commandList, CigWorkloadType(schedulingMode));

    nvigi::Result retval = kResultOk;
    if (err == NvAPI_Status::NVAPI_NO_IMPLEMENTATION) retval = kResultDriverOutOfDate;

    return retval;
}

static nvigi::Result d3d12RestoreThreadsGpuInferenceSchedulingMode(ID3D12Device* device)
{
    auto& ctx = (*hwiD3D12::getContext());
    int err = ctx.sched.ThreadSetD3DWorkloadType(device, CigWorkloadType::CIG_WORKLOAD_FOREGROUND);

    nvigi::Result retval = kResultOk;
    if (err == NvAPI_Status::NVAPI_NO_IMPLEMENTATION) retval = kResultDriverOutOfDate;

    return retval;
}

static nvigi::Result d3d12NotifyOutOfBandCommandQueue(ID3D12CommandQueue* queue, OutOfBandCommandQueueType type)
{
    auto& ctx = (*hwiD3D12::getContext());
    int err = NvAPI_D3D12_NotifyOutOfBandCommandQueue(queue, static_cast<NV_OUT_OF_BAND_CQ_TYPE>(type));
    nvigi::Result retval = kResultOk;
    if (err == NvAPI_Status::NVAPI_NO_IMPLEMENTATION) retval = kResultDriverOutOfDate;
    return retval;
}

static nvigi::Result d3d12InitScheduler(ID3D12Device* device)
{
    auto& ctx = (*hwiD3D12::getContext());
    nvigi::Result retval = kResultOk;

    bool found = (ctx.initializedDevices.find(device) != ctx.initializedDevices.end());
    if (!found)
    {
        int err = ctx.sched.InitD3DScheduler(device);
        if (err == NvAPI_Status::NVAPI_OK)
        {
            ctx.initializedDevices.insert(device);
        }
        else
        {
            retval = kResultDriverOutOfDate;
        }
    }
    return retval;
}

//! Making sure our implementation is covered with our exception handler
//! 
namespace hwiD3D12
{
static nvigi::Result ApplyGlobalGpuInferenceSchedulingModeToThread(ID3D12Device* device)
{
    NVIGI_CATCH_EXCEPTION(d3d12ApplyGlobalGpuInferenceSchedulingModeToThread(device));
}
static nvigi::Result RestoreThreadsGpuInferenceSchedulingMode(ID3D12Device* device)
{
    NVIGI_CATCH_EXCEPTION(d3d12RestoreThreadsGpuInferenceSchedulingMode(device));
}
static nvigi::Result NotifyOutOfBandCommandQueue(ID3D12CommandQueue* queue, OutOfBandCommandQueueType type)
{
    NVIGI_CATCH_EXCEPTION(d3d12NotifyOutOfBandCommandQueue(queue, type));
}
static nvigi::Result InitScheduler(ID3D12Device* device)
{
    NVIGI_CATCH_EXCEPTION(d3d12InitScheduler(device));
}
static nvigi::Result ApplyGlobalGpuInferenceSchedulingModeToCommandList(ID3D12GraphicsCommandList* commandList)
{
    NVIGI_CATCH_EXCEPTION(d3d12ApplyGlobalGpuInferenceSchedulingModeToCommandList(commandList));
}
} // namespace hwiD3D12

//! Main entry point - get information about our plugin
//! 
Result nvigiPluginGetInfo(nvigi::framework::IFramework* framework, nvigi::plugin::PluginInfo** _info)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    // Internal API, we know that incoming pointer is always valid
    auto& info = plugin::getContext()->info;
    *_info = &info;

    info.id = plugin::hwi::d3d12::kId;
    info.description = "D3D12 context plugin";
    info.author = "NVIDIA";
    info.build = GIT_BRANCH_AND_LAST_COMMIT;
    info.interfaces = { plugin::getInterfaceInfo<IHWID3D12>() };
    info.requiredVendor = VendorId::eNVDA;

    //! Specify minimum spec for the OS, driver and GPU architecture
    //! 
    info.minDriver = { 555, 85, 0 }; // CUDA in Graphics min spec
    info.minOS = { NVIGI_DEF_MIN_OS_MAJOR, NVIGI_DEF_MIN_OS_MINOR, NVIGI_DEF_MIN_OS_BUILD };
    info.minGPUArch = { NVIGI_CUDA_MIN_GPU_ARCH };

    nvigi::system::ISystem* system;
    framework::getInterface(framework, nvigi::core::framework::kId, &system);
    if (!system)
    {
        NVIGI_LOG_ERROR("NVIGI is in invalid state - missing core interface 'nvigi::system::ISystem'");
        return kResultMissingInterface;
    }
    extra::ScopedTasks releaseSystem([framework, system]() {framework::releaseInterface(framework, nvigi::core::framework::kId, system); });
    return kResultOk;
}

//! Main entry point - starting our plugin
//! 
Result nvigiPluginRegister(framework::IFramework* framework)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    auto& ctx = (*hwiD3D12::getContext());

    //! Add your interface(s) to the framework
    //!
    //! Note that we are exporting functions with the exception handler enabled
    ctx.api.d3d12ApplyGlobalGpuInferenceSchedulingModeToThread = hwiD3D12::ApplyGlobalGpuInferenceSchedulingModeToThread;
    ctx.api.d3d12RestoreThreadsGpuInferenceSchedulingMode = hwiD3D12::RestoreThreadsGpuInferenceSchedulingMode;
    ctx.api.d3d12NotifyOutOfBandCommandQueue = hwiD3D12::NotifyOutOfBandCommandQueue;
    ctx.api.d3d12InitScheduler = hwiD3D12::InitScheduler;
    ctx.api.d3d12ApplyGlobalGpuInferenceSchedulingModeToCommandList = hwiD3D12::ApplyGlobalGpuInferenceSchedulingModeToCommandList;

    framework->addInterface(plugin::hwi::d3d12::kId, &ctx.api, 0);

    // This is a dependency so make sure to provide path to dependencies if SDK is split up in different locations
    auto dependenciesPath = framework->getUTF8PathToDependencies();
    if (dependenciesPath.empty())
    {
        // Path to dependencies not provided, in this scenario hwi.common plugin must be next to this plugin
        dependenciesPath = extra::utf16ToUtf8(file::getModulePath().c_str()).c_str();
    }
    if (!framework::getInterface(plugin::getContext()->framework, nvigi::plugin::hwi::common::kId, &ctx.hwiCommon, dependenciesPath.c_str()))
    {
        NVIGI_LOG_ERROR("Get plugin::hwi::common interface failed");
        return kResultMissingDynamicLibraryDependency;
    }

    // Always load DLLs with full path and check signatures in production
    auto dependenciesPathW = extra::utf8ToUtf16(dependenciesPath.c_str());
#ifdef NVIGI_PRODUCTION
    if (!security::verifyEmbeddedSignature((dependenciesPathW + L"/cig_scheduler_settings.dll").c_str()))
    {
        NVIGI_LOG_ERROR("'cig_scheduler_settings.dll' must be digitally signed");
        return kResultInvalidState;
    }
#endif
    ctx.cigHelper = LoadLibraryW((dependenciesPathW + L"/cig_scheduler_settings.dll").c_str());
    if (ctx.cigHelper)
    {
        ctx.sched.StreamSetWorkloadType = (PFun_StreamSetWorkloadType*)GetProcAddress(ctx.cigHelper, "StreamSetWorkloadType");
        ctx.sched.ContextSetDefaultWorkloadType = (PFun_ContextSetDefaultWorkloadType*)GetProcAddress(ctx.cigHelper, "ContextSetDefaultWorkloadType");
        ctx.sched.StreamGetWorkloadType = (PFun_StreamGetWorkloadType*)GetProcAddress(ctx.cigHelper, "StreamGetWorkloadType");
        ctx.sched.ContextGetDefaultWorkloadType = (PFun_ContextGetDefaultWorkloadType*)GetProcAddress(ctx.cigHelper, "ContextGetDefaultWorkloadType");
        ctx.sched.WorkloadTypeGetName = (PFun_WorkloadTypeGetName*)GetProcAddress(ctx.cigHelper, "WorkloadTypeGetName");
        ctx.sched.ThreadSetD3DWorkloadType = (PFun_ThreadSetD3DWorkloadType*)GetProcAddress(ctx.cigHelper, "ThreadSetD3DWorkloadType");
        ctx.sched.CommandListSetD3DWorkloadType = (PFun_CommandListSetD3DWorkloadType*)GetProcAddress(ctx.cigHelper, "CommandListSetD3DWorkloadType");
        ctx.sched.InitD3DScheduler = (PFun_InitD3DScheduler*)GetProcAddress(ctx.cigHelper, "InitD3DScheduler");

        if (!ctx.sched.CommandListSetD3DWorkloadType)
        {
            NVIGI_LOG_ERROR("cig_scheduler_settings.dll does not contain CommandListSetD3DWorkloadType, please use a newer version");
            return kResultMissingDynamicLibraryDependency;
        }
        if (!ctx.sched.InitD3DScheduler)
        {
            NVIGI_LOG_ERROR("cig_scheduler_settings.dll does not contain InitD3DScheduler, please use a newer version");
            return kResultMissingDynamicLibraryDependency;
        }
    }
    else
    {
        return kResultMissingDynamicLibraryDependency;
    }

    return kResultOk;
}

//! Main exit point - shutting down our plugin
//! 
Result nvigiPluginDeregister()
{
    auto& ctx = (*hwiD3D12::getContext());

    framework::releaseInterface(plugin::getContext()->framework, nvigi::plugin::hwi::common::kId, ctx.hwiCommon);

    // We know this is a valid handle otherwise plugin register would have failed
    FreeLibrary(ctx.cigHelper);

    //! Do any other shutdown tasks here
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

}; // namespace nvigi