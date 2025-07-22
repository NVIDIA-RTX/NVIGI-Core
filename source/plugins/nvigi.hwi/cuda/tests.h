// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/plugins/nvigi.hwi/cuda/nvigi_hwi_cuda.h"
#include "source/utils/nvigi.cig_compatibility_checker/CIG_compatibility_checker.h"
#include <cuda_runtime.h>
#include <cuda.h>

namespace nvigi
{
namespace hwiCuda
{

TEST_CASE("hwi_cuda", "[hwi],[cuda]")
{
    bool runTest = true;
    bool compatChecker = false;

    if (!nvigi::params.useCiG)
        runTest = false;

    nvigi::Result res = nvigi::kResultOk;

    //! Use global params as needed (see source/tests/ai/main.cpp for details and add modify if required)
    CUdevice device{};
    CUcontext cuCtx = nullptr;
    if (runTest)
    {
        REQUIRE(nvigi::params.icig != nullptr);

        // Set the current context to a known value, the primary context        
        CUcontext contextBeforeCreate = nullptr;
        CUresult cuerr = cuDeviceGet(&device, 0);
        REQUIRE(cuerr == CUDA_SUCCESS);
        cuerr = cuDevicePrimaryCtxRetain(&contextBeforeCreate, device);
        REQUIRE(cuerr == CUDA_SUCCESS);
        cuerr = cuCtxSetCurrent(contextBeforeCreate);
        REQUIRE(cuerr == CUDA_SUCCESS);

        nvigi::D3D12Parameters d3dParams = CIGCompatibilityChecker::init(params.nvigiLoadInterface, params.nvigiUnloadInterface);

        res = nvigi::params.icig->cudaGetSharedContextForQueue(d3dParams, &cuCtx);
        if (res == nvigi::kResultOk)
            compatChecker = true;

        // We don't want cudaGetSharedContextForQueue to change the current CUDA
        // context, we want users to explicitly pass the CIG context to plugins
        // they want to run with CIG. So we check that it doesn't change the 
        // current context here.
        CUcontext contextAfterCreate{};
        cuerr = cuCtxGetCurrent(&contextAfterCreate);
        REQUIRE(cuerr == CUDA_SUCCESS);
        REQUIRE(contextAfterCreate == contextBeforeCreate);

        bool successOrOldDriver = (res == nvigi::kResultOk) || (res == nvigi::kResultDriverOutOfDate);
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

    if (compatChecker)
    {
        REQUIRE(CIGCompatibilityChecker::check());
    }

    if (runTest)
    {
        res = nvigi::params.icig->cudaReleaseSharedContext(cuCtx);
        REQUIRE(res == nvigi::kResultOk);
        CUresult err = cuDevicePrimaryCtxRelease(device);
        REQUIRE(err == CUDA_SUCCESS);
    }
}

//! Add more test cases as needed

}
}
