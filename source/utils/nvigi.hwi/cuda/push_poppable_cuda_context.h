// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "runtime_context_scope.h"

#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.api/nvigi_cuda.h"
#include "source/core/nvigi.api/nvigi_result.h"
#include "source/plugins/nvigi.hwi/cuda/nvigi_hwi_cuda.h"

#include <cuda.h>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <assert.h>

namespace nvigi
{

    // Originally, our plugins weren't aware of CUDA contexts, they just ran on the
    // current CUDA context set by the user. CIG can't be enabled on the default 
    // context though, so we had to add the ability to run plugins on a given 
    // context. The PushPoppableCudaContext class defined below is intended to make
    // adding this ability easier and less error prone.
    // 
    // CUDA has the concept of a current context (per thread). If any of our APIs 
    // change the current CUDA context, for example to the CIG CUDA context, then 
    // we must restore it to the original context before returning to the user.
    //
    // The CUDA API provides cuCtxPushCurrent and cuCtxPopCurrent for this purpose,
    // but it's easy to forget to pop, push twice, or pop without having pushed.
    //
    // The struct defined below, CudaContextPushPop, wraps the CUDA context, and 
    // tracks whether it is active (has been pushed). Its purpose is to report 
    // errors if a context is pushed when it is already active, or popped when it 
    // is not active. We also provide an RAII class, RuntimeContextScope, to make 
    // forgetting to pop impossible.
    //
    // A slight complication is that we have code that can run either on the eval 
    // thread or a worker thread, threads can be spawned between a push and a pop,
    // and Cuda tracks current context per thread. For this reason we have to track
    // whether or not our context is active (has been pushed) per thread. We store 
    // this in an unordered_set, but protect it with a mutex to make it threadsafe.

    struct PushPoppableCudaContext
    {
        bool useCudaCtx = false;
        bool cudaCtxNeedsRelease = false;
        bool constructorSucceeded = false;
        std::unordered_set<std::thread::id> threadsThatHavePushed;
        std::mutex threadsThatHavePushedMutex;
        CUcontext cudaCtx{};
        nvigi::IHWICuda* icig{};

        PushPoppableCudaContext(const nvigi::NVIGIParameter* params)
        {
            auto cudaParams = findStruct<nvigi::CudaParameters>(params);
#ifdef NVIGI_WINDOWS
            auto d3dParams = findStruct<nvigi::D3D12Parameters>(params);

            if (d3dParams && d3dParams->queue)
            {
                bool ok = framework::getInterface(plugin::getContext()->framework, nvigi::plugin::hwi::cuda::kId, &icig);
                if (!ok)
                {
                    NVIGI_LOG_ERROR("nvigiGetInterface hwi::cuda failed");
                    constructorSucceeded = false;
                    return;
                }
                nvigi::Result result = icig->cudaGetSharedContextForQueue(*d3dParams, &cudaCtx);
                if (result != nvigi::kResultOk)
                {
                    NVIGI_LOG_ERROR("cudaGetSharedContextForQueue failed, return code %d", result);
                    constructorSucceeded = false;
                    return;
                }

                useCudaCtx = true;
                cudaCtxNeedsRelease = false;
            }
            else
#endif
            if (cudaParams && cudaParams->context)
            {
                cudaCtx = cudaParams->context;
                useCudaCtx = true;
                cudaCtxNeedsRelease = false;
            }
            else
            {
                CUcontext currentCudaContext{};

                NVIGI_LOG_WARN(
                    "createInstance was called without specifying either a D3D command queue, a "
                    "Vulkan device or a CUDA context. For games, and other applications that use "
                    "both GPU graphics and compute, please chain() a D3D12Parameters or "
                    "VulkanParameters struct to allow compute and graphics to run in parallel. For "
                    "non-graphical applications, or if you want to create the CIG context yourself, "
                    "please chain() a CudaParameters struct instead.");

                NVIGI_LOG_WARN(
                    "Defaulting to CUDA device 0 with serialized compute and graphics");

                CUresult cuerr = cuInit(0);
                if (cuerr != CUDA_SUCCESS)
                {
                    NVIGI_LOG_ERROR("cuInit, return code %d", cuerr);
                    constructorSucceeded = false;
                    return;
                }

                // The user hasn't given us any information about where to run, so just run 
                // on the primary context of CUDA device 0. If they want to run somewhere 
                // else then they can either pass us a CUDA context, or a D3D device/queue.
                CUdevice cuDeviceZero{};
                cuerr = cuDeviceGet(&cuDeviceZero, 0);
                if (cuerr != CUDA_SUCCESS)
                {
                    NVIGI_LOG_ERROR("cuDeviceGet failed, return code %d", cuerr);
                    constructorSucceeded = false;
                    return;
                }
                cuerr = cuDevicePrimaryCtxRetain(&currentCudaContext, cuDeviceZero);
                if (cuerr != CUDA_SUCCESS)
                {
                    NVIGI_LOG_ERROR("cuDevicePrimaryCtxRetain failed, return code %d", cuerr);
                    constructorSucceeded = false;
                    return;
                }

                cudaCtx = currentCudaContext;
                useCudaCtx = true;
                cudaCtxNeedsRelease = true;
            }
            constructorSucceeded = true;
        }

        ~PushPoppableCudaContext()
        {
            if (cudaCtxNeedsRelease)
            {
                // If user didn't give us a context, then we retained device 0's primary 
                // context so we need to release it here
                CUdevice deviceZero;
                CUresult cuerr = cuDeviceGet(&deviceZero, 0);
                assert(cuerr == CUDA_SUCCESS && "cuDeviceGet failed");
                if (cuerr != CUDA_SUCCESS)
                {
                    NVIGI_LOG_ERROR("cuDeviceGet failed, return code %d", cuerr);
                    assert(cuerr == CUDA_SUCCESS && "cuDeviceGet failed");
                    return;
                }
                cuerr = cuDevicePrimaryCtxRelease(deviceZero);
                assert(cuerr == CUDA_SUCCESS && "cuDevicePrimaryCtxRelease failed");
                if (cuerr != CUDA_SUCCESS)
                {
                    NVIGI_LOG_ERROR("cuDevicePrimaryCtxRelease failed, return code %d", cuerr);
                    assert(cuerr == CUDA_SUCCESS && "cuDevicePrimaryCtxRelease failed");
                    return;
                }
            }

            if (icig)
            {
                bool ok = framework::releaseInterface(plugin::getContext()->framework, nvigi::plugin::hwi::cuda::kId, icig);
                if (!ok)
                {
                    NVIGI_LOG_ERROR("framework::releaseInterface failed");
                    assert(0);
                }
            }
        }

        void pushRuntimeContext()
        {
            if (useCudaCtx)
            {
                bool alreadyPushed;
                {
                    const std::lock_guard<std::mutex> lock(threadsThatHavePushedMutex);
                    auto iter = threadsThatHavePushed.find(std::this_thread::get_id());
                    alreadyPushed = (iter != threadsThatHavePushed.end());
                }
                if (alreadyPushed)
                {
                    NVIGI_LOG_ERROR("Pushing CUDA context when it is already active");
                    assert(false && "Pushing CUDA context when it is already active");
                    return;
                }
                else if (CUDA_SUCCESS != cuCtxPushCurrent(cudaCtx))
                {
                    NVIGI_LOG_ERROR("Pushing CUDA context failed");
                    assert(false && "Pushing CUDA context failed");
                    return;
                }

                {
                    const std::lock_guard<std::mutex> lock(threadsThatHavePushedMutex);
                    threadsThatHavePushed.insert(std::this_thread::get_id());
                }
            }
        }
        void popRuntimeContext()
        {
            if (useCudaCtx)
            {
                CUcontext oldCtx{};

                bool wasPushed;
                {
                    const std::lock_guard<std::mutex> lock(threadsThatHavePushedMutex);
                    auto iter = threadsThatHavePushed.find(std::this_thread::get_id());
                    wasPushed = (iter != threadsThatHavePushed.end());
                }
                if (!wasPushed)
                {
                    NVIGI_LOG_ERROR("Popping CUDA context when it was not active");
                    assert(false && "Popping CUDA context when it was not active");
                    return;
                }
                else if (CUDA_SUCCESS != cuCtxPopCurrent(&oldCtx))
                {
                    NVIGI_LOG_ERROR("Popping CUDA context failed");
                    assert(false && "Popping CUDA context failed");
                    return;
                }
                if (oldCtx != cudaCtx)
                {
                    NVIGI_LOG_ERROR("Popping the wrong CUDA context");
                    assert(false && "Popping the wrong CUDA context");
                    return;
                }

                {
                    const std::lock_guard<std::mutex> lock(threadsThatHavePushedMutex);
                    threadsThatHavePushed.erase(std::this_thread::get_id());
                }
            }
        }
    };

}