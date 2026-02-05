// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Modern AI Plugin Template
// =========================
// This template demonstrates how to create an AI inference plugin using the
// modern C++ plugin_base_ai.hpp framework. It provides a clean, type-safe API
// with support for CUDA, D3D12, and Vulkan backends.
//
// Quick Start:
// 1. Replace all instances of "TemplateAI" with your plugin name
// 2. Update the plugin ID (run: .\tools\nvigi.tool.utils.exe --plugin nvigi.plugin.your_name)
// 3. Update GUIDs for all structures (run: .\tools\nvigi.tool.utils.exe --interface YourInterfaceName)
// 4. Define PLUGIN_USES_CUDA, PLUGIN_USES_D3D12, or PLUGIN_USES_VULKAN as needed
// 5. Implement your inference logic in the TemplateAIPlugin class
//

#pragma once

#include "nvigi_ai.h"

namespace nvigi
{
namespace plugin
{

// ============================================================================
// Plugin ID
// ============================================================================
// TODO: Run .\tools\nvigi.tool.utils.exe --plugin nvigi.plugin.your_name.your_backend
//       and paste the generated ID here
namespace template_ai
{
    constexpr PluginID kId = { {0xa1b2c3d4, 0xe5f6, 0x7890, {0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90}}, 0x123456 };
}

} // namespace plugin

// ============================================================================
// Input/Output Slot Names
// ============================================================================
// Define semantic names for your plugin's input and output slots
constexpr const char* kTemplateAIInputPrompt = "prompt";
constexpr const char* kTemplateAIOutputResponse = "response";

// ============================================================================
// Creation Parameters
// ============================================================================
// IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
// 
// Run .\tools\nvigi.tool.utils.exe --interface TemplateAICreationParameters and paste new struct here
//
// TODO: Replace this GUID with a new one
struct alignas(8) TemplateAICreationParameters
{
    TemplateAICreationParameters() = default;
    NVIGI_UID(UID({ 0x11111111, 0x2222, 0x3333, {0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb} }), kStructVersion1)

    // v1 members - DO NOT break C ABI compatibility:
    // * Do not use virtual functions, volatile, STL containers (std::vector, etc.)
    // * Do not use nested structures, always use pointer members
    // * Do not use internal types in public interfaces
    // * Do not change or move existing members once interface has shipped

    // Example: Custom parameter for your plugin
    // float temperature = 1.0f;
    // int32_t maxTokens = 512;

    // v2+ members go here - remember to update kStructVersionN in NVIGI_UID above!
};

NVIGI_VALIDATE_STRUCT(TemplateAICreationParameters)

// ============================================================================
// Extended Creation Parameters (Optional)
// ============================================================================
// Use this for optional parameters that can be chained via _next member
//
// IMPORTANT: DO NOT DUPLICATE GUIDs - ALWAYS ASSIGN NEW GUID
//
// TODO: Replace this GUID with a new one
struct alignas(8) TemplateAICreationParametersEx
{
    TemplateAICreationParametersEx() = default;
    NVIGI_UID(UID({ 0x22222222, 0x3333, 0x4444, {0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc} }), kStructVersion1)

    // Optional parameters go here
    // This structure is chained to TemplateAICreationParameters using the _next member

    // Example:
    // bool enableOptimization = true;
    // const char* utf8CustomConfigPath = nullptr;
};

NVIGI_VALIDATE_STRUCT(TemplateAICreationParametersEx)

// ============================================================================
// Capabilities and Requirements
// ============================================================================
// IMPORTANT: DO NOT DUPLICATE GUIDs - ALWAYS ASSIGN NEW GUID
//
// TODO: Replace this GUID with a new one
struct alignas(8) TemplateAICapabilitiesAndRequirements
{
    TemplateAICapabilitiesAndRequirements() = default;
    NVIGI_UID(UID({ 0x33333333, 0x4444, 0x5555, {0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd} }), kStructVersion1)

    // Pointer to common capabilities and requirements
    CommonCapabilitiesAndRequirements* common = nullptr;

    // Plugin-specific capabilities can be added here
    // Example:
    // bool supportsStreaming = false;
    // uint32_t maxBatchSize = 1;
};

NVIGI_VALIDATE_STRUCT(TemplateAICapabilitiesAndRequirements)

using ITemplateAI = InferenceInterface;

} // namespace nvigi
