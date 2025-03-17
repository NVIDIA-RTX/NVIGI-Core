// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_struct.h"

// Forward declarations from dx12 headers
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;
struct D3D12_HEAP_PROPERTIES;
struct D3D12_RESOURCE_DESC;
struct D3D12_CLEAR_VALUE;

enum D3D12_RESOURCE_STATES : int;
enum D3D12_HEAP_FLAGS : int;

namespace nvigi
{

// Example definition of the d3d12 callbacks when passed as D3D12Parameters:
//
// NOTE: "REFIID riidResource" is not passed as a parameter as the ID3D12Resource has a fixed UID derived with the IID_PPV_ARGS macro
// 
// ID3D12Resource* createCommittedResource(
//     ID3D12Device* device, const D3D12_HEAP_PROPERTIES* pHeapProperties,
//     D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc,
//     D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue,
//     void* userContext
// )
// {
//    ID3D12Resource* resource = nullptr;
//    HRESULT hr = device->CreateCommittedResource(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, IID_PPV_ARGS(&resource));
//    if (FAILED(hr))
//    {
//        // Handle error
//        return nullptr;
//    }
//    if(userContext)
//    {
//        // Do something with userContext
//    }
//    return resource;
// }
//
// void destroyResource(ID3D12Resource* pResource, void* userContext)
// {
//     pResource->Release();
// }
//
// D3D12Parameters params = {};
// params.createCommittedResourceCallback = createCommittedResource;
// params.destroyResourceCallback = destroyResource;
// params.createCommitResourceUserContext = nullptr;
// params.destroyResourceUserContext = nullptr;

using PFun_createCommittedResource = ID3D12Resource*(
    ID3D12Device* device, const D3D12_HEAP_PROPERTIES* pHeapProperties, 
    D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc, 
    D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, void* userContext
    );
using PFun_destroyResource = void(ID3D12Resource* pResource, void* userContext);

// {957FF4D8-BF82-4FE4-B133-4C44764F2F77}
struct alignas(8) D3D12Parameters {
    D3D12Parameters() {}; 
    NVIGI_UID(UID({ 0x957ff4d8, 0xbf82, 0x4fe4,{ 0xb1, 0x33, 0x4c, 0x44, 0x76, 0x4f, 0x2f, 0x77 } }), kStructVersion2)
    ID3D12Device* device{};
    ID3D12CommandQueue* queue{}; // direct (graphics) queue
    // v2
    ID3D12CommandQueue* queueCompute{};
    ID3D12CommandQueue* queueCopy{};
    PFun_createCommittedResource* createCommittedResourceCallback{};
    PFun_destroyResource* destroyResourceCallback{};
    void* createCommitResourceUserContext{};
    void* destroyResourceUserContext{};
};

NVIGI_VALIDATE_STRUCT(D3D12Parameters);

//! Interface D3D12Data
//!
//! {4A51AF62-7C2C-41F6-9AA6-B19419084E0D}
struct alignas(8) D3D12Data {
    D3D12Data() {}; 
    NVIGI_UID(UID({ 0x4a51af62, 0x7c2c, 0x41f6,{ 0x9a, 0xa6, 0xb1, 0x94, 0x19, 0x08, 0x4e, 0x0d } }), kStructVersion2)
    ID3D12Resource* resource {};    
    //! v2    
    uint32_t state{}; // D3D12_RESOURCE_STATES
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(D3D12Data)

} // namespace nvigi