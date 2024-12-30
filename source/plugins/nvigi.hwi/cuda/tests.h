// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/plugins/nvigi.hwi/cuda/nvigi_hwi_cuda.h"
#include <cuda_runtime.h>
#include <cuda.h>

namespace nvigi
{
namespace hwiCuda
{

TEST_CASE("hwi_cuda", "[hwi],[cuda]")
{
    //! Use global params as needed (see source/tests/ai/main.cpp for details and add modify if required)
    cuInit(0);
    D3D12ContextInfo* d3dInfo = D3D12ContextInfo::CreateD3D12WindowAndSwapchain(100, 100, false);
    REQUIRE(d3dInfo != nullptr);

    nvigi::IHWICuda* icuda{};
    auto res = nvigiGetInterfaceDynamic(plugin::hwi::cuda::kId, &icuda, params.nvigiLoadInterface);
    bool successOrOldDriver = (res == nvigi::kResultOk) || (res == nvigi::kResultDriverOutOfDate);
    REQUIRE(successOrOldDriver);

    bool runTest = true;
    if (res == nvigi::kResultDriverOutOfDate)
    {
        NVIGI_LOG_TEST_WARN("Driver cannot support shared CUDA context, rest of test will be skipped");
        runTest = false;
    }

    CUdevice device{};
    CUcontext cuCtx = nullptr;
    if (runTest)
    {
        REQUIRE(icuda != nullptr);

        // Set the current context to a known value, the primary context        
        CUcontext contextBeforeCreate = nullptr;        
        CUresult cuerr = cuDeviceGet(&device, 0);
        REQUIRE(cuerr == CUDA_SUCCESS);
        cuerr = cuDevicePrimaryCtxRetain(&contextBeforeCreate, device);
        REQUIRE(cuerr == CUDA_SUCCESS);
        cuerr = cuCtxSetCurrent(contextBeforeCreate);
        REQUIRE(cuerr == CUDA_SUCCESS);

        nvigi::D3D12Parameters d3dParams;
        d3dParams.device = d3dInfo->device.Get();
        d3dParams.queue = d3dInfo->d3d_queue.Get();
        res = icuda->cudaGetSharedContextForQueue(d3dParams, &cuCtx);

        // We don't want cudaGetSharedContextForQueue to change the current CUDA
        // context, we want users to explicitly pass the CIG context to plugins
        // they want to run with CIG. So we check that it doesn't change the 
        // current context here.
        CUcontext contextAfterCreate{};
        cuerr = cuCtxGetCurrent(&contextAfterCreate);
        REQUIRE(cuerr == CUDA_SUCCESS);
        REQUIRE(contextAfterCreate == contextBeforeCreate);

        successOrOldDriver = (res == nvigi::kResultOk) || (res == nvigi::kResultDriverOutOfDate);
        REQUIRE(successOrOldDriver);
        if (res == nvigi::kResultDriverOutOfDate)
        {
            NVIGI_LOG_TEST_WARN("Driver cannot support shared CUDA context, rest of test will be skipped");
            runTest = false;
        }
    }

    if (runTest)
    {
        REQUIRE(res == nvigi::kResultOk);
        REQUIRE(cuCtx != nullptr);

        CUresult err = cuCtxPushCurrent(cuCtx);
        REQUIRE(err == CUDA_SUCCESS);
        CUcontext old = nullptr;
        err = cuCtxPopCurrent(&old);
        REQUIRE(err == CUDA_SUCCESS);
    }

    delete d3dInfo;

    if (runTest)
    {
        res = icuda->cudaReleaseSharedContext(cuCtx);
        REQUIRE(res == nvigi::kResultOk);
        CUresult err = cuDevicePrimaryCtxRelease(device);
        REQUIRE(err == CUDA_SUCCESS);
    }

    res = params.nvigiUnloadInterface(plugin::hwi::cuda::kId, icuda);
    REQUIRE(res == nvigi::kResultOk);
}

//! Add more test cases as needed

}
}
