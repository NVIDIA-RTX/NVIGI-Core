// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_struct.h"
#include "nvigi_d3d12.h"

namespace nvigi
{
namespace plugin::hwi::d3d12
{
    constexpr PluginID kId = { {0x1c5c4c12, 0x464e, 0x460a,{0x8c, 0x16, 0x5d, 0xde, 0xe7, 0xa4, 0x52, 0x59}}, 0xe6400d }; //{1C5C4C12-464E-460A-8C16-5DDEE7A45259} [nvigi.plugin.hwi.d3d12]
}

enum class OutOfBandCommandQueueType : int
{
    kRender = 0,
    kPresent = 1,
    kIgnore = 2,
    kRenderPresent = 3,
};

//! {EAE8496C-327C-4FEB-8940-2A8C63CB9A6A}
struct alignas(8) IHWID3D12
{
    IHWID3D12() {};
    NVIGI_UID(UID({ 0xeae8496c, 0x327c, 0x4feb,{0x89, 0x40, 0x2a, 0x8c, 0x63, 0xcb, 0x9a, 0x6a} }), kStructVersion4)

    // Called by plugins to apply the global scheduling mode to all work launched on the current thread
    nvigi::Result(*d3d12ApplyGlobalGpuInferenceSchedulingModeToThread)(ID3D12Device* device);
    
    // Called by plugins at end of evaluate to restore the GPU scheduling mode 
    nvigi::Result(*d3d12RestoreThreadsGpuInferenceSchedulingMode)(ID3D12Device* device);

    // v2
    nvigi::Result(*d3d12NotifyOutOfBandCommandQueue)(ID3D12CommandQueue* queue, OutOfBandCommandQueueType type);

    // v3
    // Must be called by all plugins that use D3D12 async compute
    nvigi::Result(*d3d12InitScheduler)(ID3D12Device* device);

    // v4
    // Called by plugins to apply the global scheduling mode to all work launched on the given command list
    nvigi::Result(*d3d12ApplyGlobalGpuInferenceSchedulingModeToCommandList)(ID3D12GraphicsCommandList* commandList);

    //! v5+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(IHWID3D12)

}