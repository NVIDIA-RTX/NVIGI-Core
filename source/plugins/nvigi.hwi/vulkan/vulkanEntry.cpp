// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.plugin/plugin.h"
#include "source/plugins/nvigi.hwi/vulkan/versions.h"
#include "source/plugins/nvigi.hwi/common/nvigi_hwi_common.h"
#include "_artifacts/gitVersion.h"
#include <vulkan/vulkan.h>
#include "nvigi_hwi_vulkan.h"
#include "vulkan_occupancy_priority.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace nvigi
{
namespace hwiVulkan
{
struct VulkanContext
{
    NVIGI_PLUGIN_CONTEXT_CREATE_DESTROY(VulkanContext);
    void onCreateContext() {};
    void onDestroyContext() {};
    IHWIVulkan api{};
    IHWICommon* hwiCommon{};
    HMODULE loader{};
    PFN_vkGetDeviceProcAddr getDeviceProcAddr{};
    bool overridePriority{};
    float priority = VK_COMPUTE_OCCUPANCY_PRIORITY_NORMAL_NV;
    float throttling = 0.0f;
};
}

NVIGI_PLUGIN_DEFINE("nvigi.plugin.hwi.vulkan", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH),
    Version(API_MAJOR, API_MINOR, API_PATCH), hwiVulkan, VulkanContext)

static bool readEnvironmentValue(const char* name, float& result) {
    const char* value = std::getenv(name);
    if (!value)
        return false;

    errno = 0;
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (end == value || *end != '\0' || errno == ERANGE ||
        !std::isfinite(parsed) || parsed < 0.0f || parsed > 1.0f) {
        NVIGI_LOG_WARN("%s='%s' must be a finite number in [0, 1]", name, value);
        return false;
    }
    result = parsed;
    return true;
}

static nvigi::Result vulkanApplyGlobalGpuInferenceSchedulingModeToCommandList(
    VkDevice device, VkCommandBuffer commandBuffer)
{
    if (!device || !commandBuffer)
        return kResultInvalidParameter;

    auto& ctx = *hwiVulkan::getContext();
    const auto command = reinterpret_cast<PFN_vkCmdSetComputeOccupancyPriorityNV>(
        ctx.getDeviceProcAddr(device, "vkCmdSetComputeOccupancyPriorityNV"));
    if (!command)
        return kResultDriverOutOfDate;

    uint32_t mode = SchedulingMode::kBalance;
    float priority = ctx.priority;
    if (!ctx.overridePriority)
    {
        const auto result = ctx.hwiCommon->GetGpuInferenceSchedulingMode(&mode);
        if (result != kResultOk)
            return result;
        if (mode == SchedulingMode::kPrioritizeGraphics)
            priority = VK_COMPUTE_OCCUPANCY_PRIORITY_LOW_NV;
        else if (mode == SchedulingMode::kPrioritizeCompute)
            priority = VK_COMPUTE_OCCUPANCY_PRIORITY_HIGH_NV;
    }
    const VkComputeOccupancyPriorityParametersNV parameters = {
        VK_STRUCTURE_TYPE_COMPUTE_OCCUPANCY_PRIORITY_PARAMETERS_NV,
        nullptr, priority, ctx.throttling
    };
    command(commandBuffer, &parameters);
    return kResultOk;
}

namespace hwiVulkan
{
static nvigi::Result ApplyGlobalGpuInferenceSchedulingModeToCommandList(
    VkDevice device, VkCommandBuffer commandBuffer)
{
    NVIGI_CATCH_EXCEPTION(vulkanApplyGlobalGpuInferenceSchedulingModeToCommandList(device, commandBuffer));
}
}

Result nvigiPluginGetInfo(framework::IFramework* framework, plugin::PluginInfo** infoOut)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;
    auto& info = plugin::getContext()->info;
    *infoOut = &info;
    info.id = plugin::hwi::vulkan::kId;
    info.description = "Vulkan GPU inference scheduling plugin";
    info.author = "NVIDIA";
    info.build = GIT_BRANCH_AND_LAST_COMMIT;
    info.interfaces = { plugin::getInterfaceInfo<IHWIVulkan>() };
    info.requiredVendor = VendorId::eNVDA;
    // Extension availability is checked for the supplied device on each call.
    info.minDriver = { 555, 85, 0 };
    info.minOS = { NVIGI_DEF_MIN_OS_MAJOR, NVIGI_DEF_MIN_OS_MINOR, NVIGI_DEF_MIN_OS_BUILD };
    info.minGPUArch = { NVIGI_CUDA_MIN_GPU_ARCH };
    return kResultOk;
}

Result nvigiPluginRegister(framework::IFramework* framework)
{
    if (!plugin::internalPluginSetup(framework)) return kResultInvalidState;
    auto& ctx = (*hwiVulkan::getContext());
    ctx.loader = LoadLibraryW(L"vulkan-1.dll");
    if (!ctx.loader)
        return kResultMissingDynamicLibraryDependency;
    ctx.getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        GetProcAddress(ctx.loader, "vkGetDeviceProcAddr"));
    if (!ctx.getDeviceProcAddr)
    {
        FreeLibrary(ctx.loader);
        ctx.loader = nullptr;
        return kResultMissingDynamicLibraryDependency;
    }
    ctx.priority = VK_COMPUTE_OCCUPANCY_PRIORITY_NORMAL_NV;
    ctx.throttling = 0.0f;
    ctx.overridePriority = readEnvironmentValue("GGML_VK_COMPUTE_OCCUPANCY_PRIORITY", ctx.priority);
    readEnvironmentValue("GGML_VK_COMPUTE_OCCUPANCY_THROTTLING", ctx.throttling);
    auto dependenciesPath = framework->getUTF8PathToDependencies();
    if (dependenciesPath.empty())
        dependenciesPath = extra::utf16ToUtf8(file::getModulePath().c_str()).c_str();
    if (!framework::getInterface(framework, plugin::hwi::common::kId,
        &ctx.hwiCommon, dependenciesPath.c_str()))
    {
        FreeLibrary(ctx.loader);
        ctx.loader = nullptr;
        ctx.getDeviceProcAddr = nullptr;
        NVIGI_LOG_ERROR("Get plugin::hwi::common interface failed");
        return kResultMissingDynamicLibraryDependency;
    }
    ctx.api.vulkanApplyGlobalGpuInferenceSchedulingModeToCommandList =
        hwiVulkan::ApplyGlobalGpuInferenceSchedulingModeToCommandList;
    framework->addInterface(plugin::hwi::vulkan::kId, &ctx.api, 0);
    return kResultOk;
}

Result nvigiPluginDeregister()
{
    auto& ctx = (*hwiVulkan::getContext());
    if (ctx.hwiCommon)
    {
        framework::releaseInterface(plugin::getContext()->framework, plugin::hwi::common::kId, ctx.hwiCommon);
        ctx.hwiCommon = nullptr;
    }
    if (ctx.loader)
        FreeLibrary(ctx.loader);
    ctx.loader = nullptr;
    ctx.getDeviceProcAddr = nullptr;
    return kResultOk;
}

NVIGI_EXPORT void* nvigiPluginGetFunction(const char* functionName)
{
    NVIGI_EXPORT_FUNCTION(nvigiPluginGetInfo);
    NVIGI_EXPORT_FUNCTION(nvigiPluginRegister);
    NVIGI_EXPORT_FUNCTION(nvigiPluginDeregister);
    return nullptr;
}
}
