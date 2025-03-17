// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_struct.h"
#include "nvigi_d3d12.h"
#include "nvigi_cuda.h"

namespace nvigi
{
namespace plugin::hwi::common
{
    constexpr PluginID kId = { {0x3126c2d9, 0xfc19, 0x4b27,{0xb8, 0xdf, 0x85, 0xbe, 0x10, 0x45, 0x0a, 0x39}}, 0x19148c }; //{3126C2D9-FC19-4B27-B8DF-85BE10450A39} [nvigi.plugin.hwi.common]
}

//! Interface 'IHWICommon'
//!
//! {425CE80B-397A-4FA3-B5BC-C6D17EEB5A26}
struct alignas(8) IHWICommon
{
    IHWICommon() { };
    NVIGI_UID(UID({ 0x425ce80b, 0x397a, 0x4fa3,{0xb5, 0xbc, 0xc6, 0xd1, 0x7e, 0xeb, 0x5a, 0x26} }), kStructVersion1)

    nvigi::Result(*SetGpuInferenceSchedulingMode)(uint32_t schedulingMode);
    nvigi::Result(*GetGpuInferenceSchedulingMode)(uint32_t* schedulingMode);
};

NVIGI_VALIDATE_STRUCT(IHWICommon)

}