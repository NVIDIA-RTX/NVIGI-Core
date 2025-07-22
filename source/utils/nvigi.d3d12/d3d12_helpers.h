// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dxgi1_6.h>

#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.system/system.h"
#include "nvigi_d3d12.h"
#include "external/agility-sdk/build/native/include/d3d12.h"
#include "source/plugins/nvigi.hwi/d3d12/nvigi_hwi_d3d12.h"

namespace nvigi
{
namespace d3d12
{

Result validateParameters(const D3D12Parameters* d3d12Params, D3D_SHADER_MODEL minSpecSM = D3D_SHADER_MODEL_6_6)
{
    if (!d3d12Params)
    {
        NVIGI_LOG_ERROR("D3D12 parameters not provided");
        return kResultInvalidParameter;
    }
    // Ensure that we have valid D3D12 device and that it supports sm 6_6 or higher
    if (!d3d12Params->device)
    {
        NVIGI_LOG_ERROR("D3D12 device not provided");
        return kResultInvalidParameter;
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16 = {};
    if (SUCCEEDED(d3d12Params->device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &options16, sizeof(options16)))) {
        if (!options16.GPUUploadHeapSupported)
        {
            NVIGI_LOG_WARN("ReBAR (D3D12_HEAP_TYPE_GPU_UPLOAD) not enabled on the system, this can degrade the performance");
        }
        else
        {
            NVIGI_LOG_INFO("ReBAR (D3D12_HEAP_TYPE_GPU_UPLOAD) enabled on the system");
        }
    }
    else
    {
        NVIGI_LOG_WARN("CheckFeatureSupport 'D3D12_FEATURE_D3D12_OPTIONS16' failed, make sure that the Agility D3D12 SDK 1.615.1 or newer is loaded");
    }
    // Ensure minSpecSM shader model or higher is supported by the provided device
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { minSpecSM };
    if (FAILED(d3d12Params->device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))))
    {
        NVIGI_LOG_ERROR("D3D12 device does not support shader model 0x%x or higher", minSpecSM);
        return kResultInvalidState;
    }
    if (shaderModel.HighestShaderModel < minSpecSM) {
        NVIGI_LOG_ERROR("Highest supported Shader Model: 0x%x (required 0x%x or higher) - please update your OS or add the latest DirectX Agility SDK to your application", shaderModel.HighestShaderModel, minSpecSM);
        return kResultInvalidState;
    }
    else
    {
        NVIGI_LOG_INFO("Detected D3D12 Shader Model 0x%x", shaderModel.HighestShaderModel);
    }
    if (d3d12Params->getVersion() >= 2)
    {
        // Check each queue and make sure it is of correct type
        D3D12_COMMAND_LIST_TYPE queueTypes[] = { D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY };
        ID3D12CommandQueue* queues[] = { d3d12Params->queue, d3d12Params->queueCompute, d3d12Params->queueCopy };
        std::string queueNames[] = { "queue", "queueCompute", "queueCopy" };
        for (size_t i = 0; i < 3; i++)
        {
            if (queues[i])
            {
                D3D12_COMMAND_QUEUE_DESC desc = queues[i]->GetDesc();
                if (desc.Type != queueTypes[i])
                {
                    NVIGI_LOG_ERROR("[D3D12Parameters::%s] is not of correct type", queueNames[i].c_str());
                    return kResultInvalidParameter;
                }
            }
            else
            {
                NVIGI_LOG_WARN("[D3D12Parameters::%s] generating internal queue", queueNames[i].c_str());
            }
        }
        if (d3d12Params->getVersion() >= 3 && (d3d12Params->flags & nvigi::D3D12ParametersFlags::eDisableReBAR))
        {
            NVIGI_LOG_WARN("ReBAR (D3D12_HEAP_TYPE_GPU_UPLOAD) disabled by user, this can degrade the performance");
        }
    }
    else
    {
        NVIGI_LOG_WARN("Using older D3D12 parameters structure, internal queue(s) will be generated resulting in limited workload scheduling control!");
    }
    return kResultOk;
}

Result getDeviceVendor(const D3D12Parameters* d3d12Params, system::ISystem* isystem, VendorId& vendor)
{
    if (NVIGI_FAILED(res, validateParameters(d3d12Params)))
    {
        return res;
    }
    
    // The D3D priority API is only available on NVDA, so check the adapter's vendor
    nvigi::LUID d3dLuid;
    d3dLuid.LowPart = d3d12Params->device->GetAdapterLuid().LowPart;
    d3dLuid.HighPart = d3d12Params->device->GetAdapterLuid().HighPart;

    const system::SystemCaps* systemCaps = isystem->getSystemCaps();

    uint32_t adapterId = 0;
    while (adapterId != systemCaps->adapterCount &&
        !(systemCaps->adapters[adapterId]->id.LowPart == d3dLuid.LowPart &&
            systemCaps->adapters[adapterId]->id.HighPart == d3dLuid.HighPart))
    {
        adapterId++;
    }
    bool found = (adapterId != systemCaps->adapterCount);

    if (!found)
    {
        NVIGI_LOG_ERROR("The requested D3D device is on adapter LUID %zu, but that LUID was not found in SystemCaps");
        return kResultItemNotFound;
    }

    system::Adapter* adapter = systemCaps->adapters[adapterId];
    vendor = adapter->vendor;

    return kResultOk;
}

Result applyNVDASpecificSettings(const D3D12Parameters* d3d12Params, IHWID3D12* iscg)
{
    if (NVIGI_FAILED(res, validateParameters(d3d12Params)))
    {
        return res;
    }

    if (!(d3d12Params->flags & nvigi::D3D12ParametersFlags::eComputeQueueSharedWithFrame))
    {
        if (NVIGI_FAILED(res, iscg->d3d12NotifyOutOfBandCommandQueue(d3d12Params->queueCompute, nvigi::OutOfBandCommandQueueType::kIgnore)))
        {
            NVIGI_LOG_WARN("d3d12NotifyOutOfBandCommandQueue call failed with result %u, this might impact performance especially in connection to ReFlex and DLSS", res);
        }
    }

    if (iscg->getVersion() >= 3)
    {
        Result result = iscg->d3d12InitScheduler(d3d12Params->device);
        if (result != kResultOk)
        {
            NVIGI_LOG_WARN_ONCE("D3D12 scheduler failed to init, requires 580 driver or higher\n");
        }
    }
    else
    {
        NVIGI_LOG_WARN_ONCE("hwi.d3d12 interface has version %d, but version 2 is required to init D3D scheduling", iscg->getVersion());
    }

    return kResultOk;
}

} // namespace d3d12
} // namespace nvigi