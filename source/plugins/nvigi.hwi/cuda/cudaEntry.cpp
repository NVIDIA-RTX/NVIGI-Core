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
#include "source/plugins/nvigi.hwi/cuda/versions.h"
#include "source/plugins/nvigi.hwi/common/nvigi_hwi_common.h"
#include "_artifacts/gitVersion.h"
#include "cig_scheduler_settings.h"

#include <cuda.h>
#include <cuda_runtime.h>

#include "cuda_scg.h"
#include "nvigi_hwi_cuda.h"

namespace nvigi
{

namespace hwiCuda
{
struct CudaContext
{
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(CudaContext);

    void onCreateContext()
    {
    };

    void onDestroyContext()
    {
    };

    IHWICuda api{};
        
    struct CudaContextInfo
    {
        CUcontext ctx{};
        int64_t refcount{};
    };

    std::map<ID3D12CommandQueue*, CudaContextInfo> contextMap;

    IHWICommon* hwiCommon;

    CigSchedulerSettingsAPI sched;

    HMODULE cigHelper{};
};
};

//! Define our plugin
//! 
//! IMPORTANT: Make sure to place this macro right after the context declaration and always within the 'nvigi' namespace ONLY.
NVIGI_PLUGIN_DEFINE("nvigi.plugin.hwi.cuda", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(API_MAJOR, API_MINOR, API_PATCH), hwiCuda, CudaContext)

static nvigi::Result cudaGetSharedContextForQueue(const nvigi::D3D12Parameters& params, CUcontext* cuCtx)
{
    auto& ctx = (*hwiCuda::getContext());

    if (cuCtx == nullptr || params.device == nullptr || params.queue == nullptr)
        return nvigi::kResultInvalidParameter;

    //! IMPORTANT: With CiG sometimes we can fail to create context with a direct (graphics) queue hence we need to try with the async compute one
    //!
    //! Logic:
    //! 
    //! * Try direct queue first, if all good ignore async
    //! * If direct fails then async compute queue becomes a mandatory parameter
    //! * If both queues fail then we have a problem but ideally that should never happen
    //! 
    auto creaateSharedContext = [&ctx](const D3D12Parameters& params, ID3D12CommandQueue** actualQueueUsed) -> nvigi::Result
        {
            CUcontext cuCtx{};
            *actualQueueUsed = params.queue;
            // Try direct queue first, we checked before that it is valid and provided
            if (!ctx.contextMap.contains(params.queue))
            {
                // Not cached, create one
                if (NVIGI_FAILED(res, nvigi::cudaScg::CreateSharedCUDAContext(params.device, params.queue, cuCtx)))
                {
                    // Failed with direct queue, let's try async compute, it becomes a mandatory parameter now
                    if (!params.queueCompute)
                    {
                        NVIGI_LOG_ERROR("Failed to create CUDA shared context with the provided direct (graphics) queue, please provide your asynchronous compute queue in D3D12Parameters");
                        return nvigi::kResultInvalidParameter;
                    }
                    *actualQueueUsed = params.queueCompute;
                    if (!ctx.contextMap.contains(params.queueCompute))
                    {
                        // Not cached, create one
                        if (NVIGI_FAILED(res, nvigi::cudaScg::CreateSharedCUDAContext(params.device, params.queueCompute, cuCtx)))
                        {
                            return res;
                        }
                    }
                }
            }
            // Check if this is the first time we create a context and cache it
            if (!ctx.contextMap.contains(*actualQueueUsed))
            {
                hwiCuda::CudaContext::CudaContextInfo info;
                info.ctx = cuCtx;
                info.refcount = 0;
                ctx.contextMap[*actualQueueUsed] = info;
            }
            return kResultOk;
        };

    ID3D12CommandQueue* actualQueueUsed = nullptr;
    if (NVIGI_FAILED(res, creaateSharedContext(params, &actualQueueUsed)))
    {
        NVIGI_LOG_ERROR("Failed to create shared CUDA context");
        return res;
    }

    hwiCuda::CudaContext::CudaContextInfo& ctxInfo = ctx.contextMap[actualQueueUsed];
    ctxInfo.refcount++;
    *cuCtx = ctxInfo.ctx;

    return kResultOk;
}

static nvigi::Result cudaReleaseSharedContext(CUcontext cuCtx)
{
    auto& ctx = (*hwiCuda::getContext());

    for (auto& [queue, queueInfo] : ctx.contextMap)
    {
        if (queueInfo.ctx == cuCtx)
        {
            queueInfo.refcount--;
            if (queueInfo.refcount <= 0)
            {
                cuCtxDestroy(cuCtx);

                ctx.contextMap.erase(queue);
            }
            return kResultOk;
        }
    }

    return kResultInvalidParameter;
}

static nvigi::Result cudaApplyGlobalGpuInferenceSchedulingMode(CUstream* cudaStreams, size_t cudaStreamsCount)
{
    if(cudaStreams == nullptr)
        return nvigi::kResultInvalidParameter;

    auto& ctx = (*hwiCuda::getContext());

    uint32_t schedulingMode;
    ctx.hwiCommon->GetGpuInferenceSchedulingMode(&schedulingMode);

    // Check that the public and private enum types are equal
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_FOREGROUND) ==
        int(SchedulingMode::kPrioritizeCompute));
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_BACKGROUND) ==
        int(SchedulingMode::kBalance));
    static_assert(int(CigWorkloadType::CIG_WORKLOAD_BACKGROUND_WITH_THROTTLE) ==
        int(SchedulingMode::kPrioritizeGraphics));

    CUresult okSoFar = CUDA_SUCCESS;
    for (size_t i = 0; okSoFar == CUDA_SUCCESS && i != cudaStreamsCount; i++)
    {
        okSoFar = ctx.sched.StreamSetWorkloadType(cudaStreams[i], CigWorkloadType(schedulingMode));
    }

    // Translate CUresult to nvigi::Result
    nvigi::Result retval = kResultOk;
    if (okSoFar != CUDA_SUCCESS) retval = kResultDriverOutOfDate;

    return retval;
}

//! Making sure our implementation is covered with our exception handler
//! 
namespace hwiCuda
{

    static nvigi::Result GetSharedContextForQueue(const nvigi::D3D12Parameters& params, CUcontext* cuCtx)
    {
        NVIGI_CATCH_EXCEPTION(cudaGetSharedContextForQueue(params, cuCtx));
    }

    static nvigi::Result ReleaseSharedContext(CUcontext cuCtx)
    {
        NVIGI_CATCH_EXCEPTION(cudaReleaseSharedContext(cuCtx));
    }

    static nvigi::Result ApplyGlobalGpuInferenceSchedulingMode(CUstream* cudaStreams, size_t cudaStreamsCount)
    {
        NVIGI_CATCH_EXCEPTION(cudaApplyGlobalGpuInferenceSchedulingMode(cudaStreams, cudaStreamsCount));
    }
} // namespace hwiCuda

//! Main entry point - get information about our plugin
//! 
Result nvigiPluginGetInfo(nvigi::framework::IFramework* framework, nvigi::plugin::PluginInfo** _info)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;
    
    // Internal API, we know that incoming pointer is always valid
    auto& info = plugin::getContext()->info;
    *_info = &info;

    info.id = plugin::hwi::cuda::kId;
    info.description = "CUDA context plugin";
    info.author = "NVIDIA";
    info.build = GIT_BRANCH_AND_LAST_COMMIT;
    info.interfaces = { plugin::getInterfaceInfo<IHWICuda>()};
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
    if (!(system->getSystemCaps()->flags & nvigi::SystemFlags::eHWSchedulingEnabled))
    {
        NVIGI_LOG_ERROR("HW scheduling in OS must be enabled in order for 'nvigi.plugin.hwi.cuda' to operate correctly");
        return kResultInvalidState;
    }
    return kResultOk;
}

//! Main entry point - starting our plugin
//! 
Result nvigiPluginRegister(framework::IFramework* framework)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    auto& ctx = (*hwiCuda::getContext());

    //! Add your interface(s) to the framework
    //!
    //! Note that we are exporting functions with the exception handler enabled
    ctx.api.cudaGetSharedContextForQueue = hwiCuda::GetSharedContextForQueue;
    ctx.api.cudaReleaseSharedContext = hwiCuda::ReleaseSharedContext;
    ctx.api.cudaApplyGlobalGpuInferenceSchedulingMode = hwiCuda::ApplyGlobalGpuInferenceSchedulingMode;

    framework->addInterface(plugin::hwi::cuda::kId, &ctx.api, 0);
    
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
    auto& ctx = (*hwiCuda::getContext());

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

}
