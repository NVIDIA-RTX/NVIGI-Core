// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.plugin/plugin.h"
#include "source/plugins/nvigi.hwi/cuda/versions.h"
#include "_artifacts/gitVersion.h"

#include <cuda.h>
#include <cuda_runtime.h>

#include "nvigi_hwi_common.h"

namespace nvigi
{
namespace hwiCommon
{
    constexpr uint32_t kDefaultSchedulingMode = SchedulingMode::kBalance;

    struct CommonContext
    {
        NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(CommonContext);

        void onCreateContext()
        {
        };

        void onDestroyContext()
        {
        };

        IHWICommon api{};
        uint32_t globalSchedulingMode = kDefaultSchedulingMode;
    };
};

NVIGI_PLUGIN_DEFINE("nvigi.plugin.hwi.common", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(API_MAJOR, API_MINOR, API_PATCH), hwiCommon, CommonContext)

// Typically called by the user
static nvigi::Result commonSetGpuInferenceSchedulingMode(uint32_t schedulingMode)
{
    auto& ctx = (*hwiCommon::getContext());

    ctx.globalSchedulingMode = schedulingMode;
    return nvigi::kResultOk;
}

// Typically called by plugins
static nvigi::Result commonGetGpuInferenceSchedulingMode(uint32_t* schedulingMode)
{
    if (schedulingMode == nullptr)
        return nvigi::kResultInvalidParameter;

    auto& ctx = (*hwiCommon::getContext());
    *schedulingMode = ctx.globalSchedulingMode;

    return nvigi::kResultOk;
}

//! Making sure our implementation is covered with our exception handler
//! 
namespace hwiCommon
{
    static nvigi::Result SetGpuInferenceSchedulingMode(uint32_t schedulingMode)
    {
        NVIGI_CATCH_EXCEPTION(commonSetGpuInferenceSchedulingMode(schedulingMode));
    }

    static nvigi::Result GetGpuInferenceSchedulingMode(uint32_t* schedulingMode)
    {
        NVIGI_CATCH_EXCEPTION(commonGetGpuInferenceSchedulingMode(schedulingMode));
    }
} // namespace hwiCommon

//! Main entry point - get information about our plugin
//! 
Result nvigiPluginGetInfo(nvigi::framework::IFramework* framework, nvigi::plugin::PluginInfo** _info)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;
    
    // Internal API, we know that incoming pointer is always valid
    auto& info = plugin::getContext()->info;
    *_info = &info;

    info.id = plugin::hwi::common::kId;
    info.description = "Common SCG plugin";
    info.author = "NVIDIA";
    info.build = GIT_BRANCH_AND_LAST_COMMIT;
    info.interfaces = { plugin::getInterfaceInfo<IHWICommon>()};
    info.requiredVendor = VendorId::eNVDA;

    //! Specify minimum spec for the OS, driver and GPU architecture
    //! 
    info.minDriver = { 555, 85, 0 }; // CUDA in Graphics min spec
    info.minOS = { NVIGI_DEF_MIN_OS_MAJOR, NVIGI_DEF_MIN_OS_MINOR, NVIGI_DEF_MIN_OS_BUILD };
    info.minGPUArch = { NVIGI_CUDA_MIN_GPU_ARCH };
    
    return kResultOk;
}

//! Main entry point - starting our plugin
//! 
Result nvigiPluginRegister(framework::IFramework* framework)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;

    auto& ctx = (*hwiCommon::getContext());

    ctx.api.SetGpuInferenceSchedulingMode = hwiCommon::SetGpuInferenceSchedulingMode;
    ctx.api.GetGpuInferenceSchedulingMode = hwiCommon::GetGpuInferenceSchedulingMode;

    framework->addInterface(plugin::hwi::common::kId, &ctx.api, 0);
    
    return kResultOk;
}

//! Main exit point - shutting down our plugin
//! 
Result nvigiPluginDeregister()
{
    auto& ctx = (*hwiCommon::getContext());

    //! Do any other shutdown tasks here
    return kResultOk;
}

//! The only exported function - gateway to all functionality
NVIGI_EXPORT void* nvigiPluginGetFunction(const char* functionName)
{
    //! Core API
    NVIGI_EXPORT_FUNCTION(nvigiPluginGetInfo);
    NVIGI_EXPORT_FUNCTION(nvigiPluginRegister);
    NVIGI_EXPORT_FUNCTION(nvigiPluginDeregister);

    return nullptr;
}

}
