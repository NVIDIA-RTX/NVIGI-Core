// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include "nvigi_struct.h"

// Dispatchable Vulkan handles, so consumers do not need Vulkan SDK headers.
struct VkDevice_T;
struct VkCommandBuffer_T;

namespace nvigi
{
namespace plugin::hwi::vulkan
{
    constexpr PluginID kId = { { 0x94944678, 0xad00, 0x49ee, {0x89, 0x24, 0x47, 0xd0, 0x4f, 0x53, 0xef, 0x7b} }, 0xd55782 }; // 94944678-ad00-49ee-8924-47d04f53ef7b [nvigi.plugin.hwi.vulkan]
}

//! {BC6A52C9-9F98-4F6F-B0B9-80DE828CB770}
struct alignas(8) IHWIVulkan
{
    IHWIVulkan() {};
    NVIGI_UID(UID({ 0xbc6a52c9, 0x9f98, 0x4f6f, {0xb0, 0xb9, 0x80, 0xde, 0x82, 0x8c, 0xb7, 0x70} }), kStructVersion1)

    // Called before inference dispatches, inside a recording Vulkan command buffer.
    // device must own commandBuffer and enable VK_NV_compute_occupancy_priority.
    // GGML_VK_COMPUTE_OCCUPANCY_PRIORITY and GGML_VK_COMPUTE_OCCUPANCY_THROTTLING
    // are read when this plugin loads and optionally override the global priority
    // and occupancy throttling (range [0, 1]).
    nvigi::Result(*vulkanApplyGlobalGpuInferenceSchedulingModeToCommandList)(
        VkDevice_T* device, VkCommandBuffer_T* commandBuffer);

    //! v2+ members go here, remember to update the kStructVersionN in NVIGI_UID!
};

NVIGI_VALIDATE_STRUCT(IHWIVulkan)
}
