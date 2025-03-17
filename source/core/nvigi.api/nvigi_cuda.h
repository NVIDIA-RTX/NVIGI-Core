// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_struct.h"

typedef int CUdevice_v1;                                     /**< CUDA device */
typedef CUdevice_v1 CUdevice;                                /**< CUDA device */
typedef struct CUctx_st* CUcontext;                          /**< CUDA context */
typedef struct CUstream_st* CUstream;                        /**< CUDA stream */

namespace nvigi
{

//! Temporary solution, do we really need to ship all this?
//! 
enum CudaImplementation
{
    eTRT,
    eGGML,
    eNative
};

namespace SchedulingMode
{
    constexpr uint32_t kPrioritizeCompute = 0;
    constexpr uint32_t kBalance = 1;
    constexpr uint32_t kPrioritizeGraphics = 2;
    constexpr uint32_t kNumOptions = 3;
};

using PFun_nvigiCudaReportCallback = void(void* ptr, size_t size, void* user_context);
using PFun_nvigiCudaMallocCallback = int32_t(void** ptr, size_t size, int device, bool managed, bool hip, void* user_context);
using PFun_nvigiCudaFreeCallback = int32_t(void* ptr, void* user_context);

struct alignas(8) CudaParameters {
    CudaParameters() {}; 
    NVIGI_UID(UID({ 0xfab2bd3f, 0x8a3e, 0x41ab,{ 0x88, 0xde, 0xd6, 0xcb, 0x2b, 0x65, 0xc5, 0x54 } }), kStructVersion2)
    CudaImplementation impl{}; // remove at some point
    CUdevice device{};
    CUcontext context{};
    CUstream stream{};

    // v2 interface
    PFun_nvigiCudaReportCallback* cudaMallocReportCallback{};
    void* cudaMallocReportUserContext{};

    PFun_nvigiCudaReportCallback* cudaFreeReportCallback{};
    void* cudaFreeReportUserContext{};

    PFun_nvigiCudaMallocCallback* cudaMallocCallback{};
    void* cudaMallocUserContext{};

    PFun_nvigiCudaFreeCallback* cudaFreeCallback{};
    void* cudaFreeUserContext{};
};

NVIGI_VALIDATE_STRUCT(CudaParameters);

//! Interface CudaData
//!
//! {DEE43A64-2622-492E-8737-9AAD6BE1D634}
struct alignas(8) CudaData {
    CudaData() {}; 
    NVIGI_UID(UID({ 0xdee43a64, 0x2622, 0x492e,{ 0x87, 0x37, 0x9a, 0xad, 0x6b, 0xe1, 0xd6, 0x34 } }), kStructVersion1)
    //! Data buffer
    const void* buffer{};
    //! Number of bytes in the buffer
    size_t sizeInBytes{};
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(CudaData)


}
