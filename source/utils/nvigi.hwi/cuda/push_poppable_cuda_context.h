// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "runtime_context_scope.h"

#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.api/nvigi_cuda.h"
#include "source/core/nvigi.api/nvigi_result.h"
#include "source/core/nvigi.api/nvigi_vulkan.h"
#include "source/plugins/nvigi.hwi/cuda/nvigi_hwi_cuda.h"

#include <cuda.h>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <assert.h>

#define CUDA_CTX_DEBUG 0

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
        bool usingCiG = false;

        PushPoppableCudaContext(const nvigi::NVIGIParameter* params)
        {
            auto cudaParams = findStruct<nvigi::CudaParameters>(params);
#ifdef NVIGI_WINDOWS
            auto d3dParams = findStruct<nvigi::D3D12Parameters>(params);
            auto vulkanParams = findStruct<nvigi::VulkanParameters>(params);

            if ((d3dParams && d3dParams->queue)|| (vulkanParams && vulkanParams->queue))
            {
                bool ok = framework::getInterface(plugin::getContext()->framework, nvigi::plugin::hwi::cuda::kId, &icig);
                if (!ok)
                {
                    NVIGI_LOG_ERROR("nvigiGetInterface hwi::cuda failed");
                    constructorSucceeded = false;
                    return;
                }

                if (d3dParams && d3dParams->queue)
                {
                    nvigi::Result result = icig->cudaGetSharedContextForQueue(*d3dParams, &cudaCtx);
                    if (result != nvigi::kResultOk)
                    {
                        NVIGI_LOG_ERROR("cudaGetSharedContextForQueue failed, return code %d", result);
                        constructorSucceeded = false;
                        return;
                    }
                }
                else if (vulkanParams && vulkanParams->queue)
                {
                    nvigi::Result result = icig->cudaGetSharedContextForVulkanQueue(*vulkanParams, &cudaCtx);
                    if (result != nvigi::kResultOk)
                    {
                        NVIGI_LOG_ERROR("cudaGetSharedContextForVulkanQueue failed, return code %d", result);
                        constructorSucceeded = false;
                        return;
                    }
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

                // TODO: Take multi GPU into account when deciding what warning to display here
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

                int cuDeviceOrdinal;
                if (cudaParams && cudaParams->device)
                {
                    // User provided a CudaParameters struct, use the device from that
                    cuDeviceOrdinal = cudaParams->device;
                }
                else
                {
                    // User didn't provide a CudaParameters struct, use default CUDA device
                    cuDeviceOrdinal = 0;
                }

                CUdevice cuDevice{};
                cuerr = cuDeviceGet(&cuDevice, cuDeviceOrdinal);
                if (cuerr != CUDA_SUCCESS)
                {
                    NVIGI_LOG_ERROR("cuDeviceGet failed, return code %d", cuerr);
                    constructorSucceeded = false;
                    return;
                }
                cuerr = cuDevicePrimaryCtxRetain(&currentCudaContext, cuDevice);
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

#ifdef NVIGI_WINDOWS
            // !cudaCtxNeedsRelease because we don't need to check the default context
            // and don't want to push/pop it
            if (useCudaCtx && !cudaCtxNeedsRelease)
            {
                CUresult cuerr = cuCtxPushCurrent(cudaCtx);
                if (cuerr != CUDA_SUCCESS)
                {
                    NVIGI_LOG_ERROR("cuCtxPushCurrent failed, return code %d", cuerr);
                }
                else
                {
                    size_t value{};
                    cuerr = cuCtxGetLimit(&value, CU_LIMIT_CIG_ENABLED);
                    if (cuerr != CUDA_SUCCESS)
                    {
                        NVIGI_LOG_ERROR("cuCtxGetLimit failed, return code %d", cuerr);
                    }
                    else
                    {
                        usingCiG = value != 0;
                    }
                    CUcontext oldCtx{};
                    cuerr = cuCtxPopCurrent(&oldCtx);
                    if (cuerr != CUDA_SUCCESS)
                    {
                        NVIGI_LOG_ERROR("cuCtxPopCurrent failed, return code %d", cuerr);
                    }
                }
            }
#endif
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

        bool isUsingCiG() { return usingCiG; }

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
                
#if CUDA_CTX_DEBUG                
                // Log the current context before pushing
                CUcontext currentCtx = nullptr;
                cuCtxGetCurrent(&currentCtx);
                NVIGI_LOG_INFO("[CUDA_CTX_DEBUG] PUSH: thread_id=0x%llx, about to push context=%p, current_context_before_push=%p", 
                               (unsigned long long)std::hash<std::thread::id>{}(std::this_thread::get_id()),
                               cudaCtx, currentCtx);
#endif                
                CUresult pushResult = cuCtxPushCurrent(cudaCtx);
                if (CUDA_SUCCESS != pushResult)
                {
                    NVIGI_LOG_ERROR("Pushing CUDA context failed, error code: %d", pushResult);
                    assert(false && "Pushing CUDA context failed");
                    return;
                }

                {
                    const std::lock_guard<std::mutex> lock(threadsThatHavePushedMutex);
                    threadsThatHavePushed.insert(std::this_thread::get_id());
                }
#if CUDA_CTX_DEBUG                 
                NVIGI_LOG_INFO("[CUDA_CTX_DEBUG] PUSH: succeeded, context=%p is now active on thread 0x%llx", 
                               cudaCtx, (unsigned long long)std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif                               
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
                
#if CUDA_CTX_DEBUG                 
                // Log the current context before popping
                CUcontext currentCtx = nullptr;
                cuCtxGetCurrent(&currentCtx);
                NVIGI_LOG_INFO("[CUDA_CTX_DEBUG] POP: thread_id=0x%llx, expected_context=%p, current_context_before_pop=%p", 
                               (unsigned long long)std::hash<std::thread::id>{}(std::this_thread::get_id()),
                               cudaCtx, currentCtx);
#endif

                CUresult popResult = cuCtxPopCurrent(&oldCtx);
                if (CUDA_SUCCESS != popResult)
                {
                    NVIGI_LOG_ERROR("Popping CUDA context failed, error code: %d", popResult);
                    assert(false && "Popping CUDA context failed");
                    return;
                }
                
#if CUDA_CTX_DEBUG                   
                // Get the context that's now current after the pop
                CUcontext newCurrentCtx = nullptr;
                cuCtxGetCurrent(&newCurrentCtx);
#endif                
                if (oldCtx != cudaCtx)
                {
#if CUDA_CTX_DEBUG                      
                    NVIGI_LOG_ERROR("[CUDA_CTX_DEBUG] CONTEXT MISMATCH DETECTED!");
                    NVIGI_LOG_ERROR("  Thread ID:                    0x%llx", 
                                    (unsigned long long)std::hash<std::thread::id>{}(std::this_thread::get_id()));
                    NVIGI_LOG_ERROR("  Expected context to pop:      %p", cudaCtx);
                    NVIGI_LOG_ERROR("  Context before pop:           %p", currentCtx);
                    NVIGI_LOG_ERROR("  Context actually popped:      %p", oldCtx);
                    NVIGI_LOG_ERROR("  Context now current after pop:%p", newCurrentCtx);
                    NVIGI_LOG_ERROR("  Using CiG:                    %s", usingCiG ? "YES" : "NO");
                    
                    // Log all threads that have pushed
                    {
                        const std::lock_guard<std::mutex> lock(threadsThatHavePushedMutex);
                        NVIGI_LOG_ERROR("  Threads with active push: %zu", threadsThatHavePushed.size());
                        for (const auto& tid : threadsThatHavePushed)
                        {
                            NVIGI_LOG_ERROR("    - Thread 0x%llx", (unsigned long long)std::hash<std::thread::id>{}(tid));
                        }
                    }
#endif                    
                    NVIGI_LOG_ERROR("Popping the wrong CUDA context");
                    assert(false && "Popping the wrong CUDA context");
                    return;
                }

                {
                    const std::lock_guard<std::mutex> lock(threadsThatHavePushedMutex);
                    threadsThatHavePushed.erase(std::this_thread::get_id());
                }
#if CUDA_CTX_DEBUG                 
                NVIGI_LOG_INFO("[CUDA_CTX_DEBUG] POP: succeeded, popped context=%p, now current=%p on thread 0x%llx", 
                               oldCtx, newCurrentCtx, (unsigned long long)std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif                               
            }
        }
    };

}