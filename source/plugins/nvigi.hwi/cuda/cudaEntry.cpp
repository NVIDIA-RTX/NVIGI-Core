// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.plugin/plugin.h"
#include "source/plugins/nvigi.hwi/cuda/versions.h"
#include "_artifacts/gitVersion.h"

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

    if (!ctx.contextMap.contains(params.queue))
    {
        CUcontext cuContext = nullptr;
        nvigi::Result res = nvigi::cudaScg::CreateSharedCUDAContext(params.device, params.queue, cuContext);
        if (res != nvigi::kResultOk)
            return res;

        hwiCuda::CudaContext::CudaContextInfo info;
        info.ctx = cuContext;
        info.refcount = 0;

        ctx.contextMap[params.queue] = info;
    }

    hwiCuda::CudaContext::CudaContextInfo& ctxInfo = ctx.contextMap[params.queue];
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

    framework->addInterface(plugin::hwi::cuda::kId, &ctx.api, 0);
    
    return kResultOk;
}

//! Main exit point - shutting down our plugin
//! 
Result nvigiPluginDeregister()
{
    auto& ctx = (*hwiCuda::getContext());

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
