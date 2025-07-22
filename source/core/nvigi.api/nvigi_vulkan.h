// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_struct.h"

//! Forward declarations
enum VkResult;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkInstance_T;
struct VkSwapchainKHR_T;
struct VkQueue_T;
struct VkBuffer_T;
struct VkDeviceMemory_T;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkDevice = VkDevice_T*;
using VkInstance = VkInstance_T*;
using VkSwapchainKHR = VkSwapchainKHR_T*;
using VkQueue = VkQueue_T*;
using VkBuffer = VkBuffer_T*;
using VkDeviceMemory = VkDeviceMemory_T*;
using VkDeviceSize = uint64_t;

namespace nvigi
{

using PFun_allocateMemoryCallback = VkResult(VkDevice device, VkDeviceSize size, uint32_t memoryTypeIndex, VkDeviceMemory* outMemory);
using PFun_freeMemoryCallback = void(VkDevice device, VkDeviceMemory memory);

//! Interface 'VulkanParameters'
//!
//! {AAA3C04F-96A5-4878-910E-3F3FAB4409E6}
struct alignas(8) VulkanParameters
{
    VulkanParameters() { };
    NVIGI_UID(UID({0xaaa3c04f, 0x96a5, 0x4878,{0x91, 0x0e, 0x3f, 0x3f, 0xab, 0x44, 0x09, 0xe6}}), kStructVersion2)

    //! IMPORTANT: All members are optional, if not provided, NVIGI will create internal device, instance and queues.

    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    VkInstance instance{};
    VkQueue queue{};                    // VK_QUEUE_GRAPHICS_BIT

    //! v2
    VkQueue queueCompute{};             // VK_QUEUE_COMPUTE_BIT, no VK_QUEUE_GRAPHICS_BIT
    VkQueue queueTransfer{};            // VK_QUEUE_TRANSFER_BIT, no VK_QUEUE_COMPUTE_BIT or VK_QUEUE_GRAPHICS_BIT

    PFun_allocateMemoryCallback* allocateMemoryCallback = nullptr;
    PFun_freeMemoryCallback* freeMemoryCallback = nullptr;
    void* allocateMemoryCallbackUserContext = nullptr;
    void* freeMemoryCallbackUserContext = nullptr;
    
    //! v3+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(VulkanParameters)

//! Interface 'VulkanData'
//!
//! {2B7A560A-A1D7-463E-86D8-B68335A6B903}
struct alignas(8) VulkanData
{
    VulkanData() { };
    NVIGI_UID(UID({0x2b7a560a, 0xa1d7, 0x463e,{0x86, 0xd8, 0xb6, 0x83, 0x35, 0xa6, 0xb9, 0x03}}), kStructVersion1)

    VkBuffer buffer{};
    size_t sizeInBytes{};
    
    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(VulkanData)


}
