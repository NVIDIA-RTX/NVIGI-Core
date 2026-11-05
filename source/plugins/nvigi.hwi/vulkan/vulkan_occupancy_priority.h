// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

// Include vulkan.h first. The SDK's Vulkan headers predate this extension.
#ifndef VK_NV_compute_occupancy_priority
#define VK_NV_compute_occupancy_priority 1
#define VK_NV_COMPUTE_OCCUPANCY_PRIORITY_SPEC_VERSION 1
#define VK_NV_COMPUTE_OCCUPANCY_PRIORITY_EXTENSION_NAME "VK_NV_compute_occupancy_priority"
#define VK_STRUCTURE_TYPE_COMPUTE_OCCUPANCY_PRIORITY_PARAMETERS_NV ((VkStructureType)1000645000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_OCCUPANCY_PRIORITY_FEATURES_NV ((VkStructureType)1000645001)
#define VK_COMPUTE_OCCUPANCY_PRIORITY_LOW_NV 0.25f
#define VK_COMPUTE_OCCUPANCY_PRIORITY_NORMAL_NV 0.50f
#define VK_COMPUTE_OCCUPANCY_PRIORITY_HIGH_NV 0.75f

typedef struct VkComputeOccupancyPriorityParametersNV {
    VkStructureType sType;
    const void* pNext;
    float occupancyPriority;
    float occupancyThrottling;
} VkComputeOccupancyPriorityParametersNV;

typedef struct VkPhysicalDeviceComputeOccupancyPriorityFeaturesNV {
    VkStructureType sType;
    void* pNext;
    VkBool32 computeOccupancyPriority;
} VkPhysicalDeviceComputeOccupancyPriorityFeaturesNV;

typedef void (VKAPI_PTR *PFN_vkCmdSetComputeOccupancyPriorityNV)(
    VkCommandBuffer commandBuffer,
    const VkComputeOccupancyPriorityParametersNV* pParameters);
#endif
