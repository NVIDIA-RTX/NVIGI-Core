// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_ai.h"

namespace nvigi
{
namespace plugin
{

//! TO DO: Run .\tools\nvigi.tool.utils.exe --plugin nvigi.plugin.my_name.my_backend.my_api and paste new id here
namespace tmpl_infer
{
    constexpr PluginID kIdBackendApi = { {0x13830f54, 0x77a0, 0x4626,{0x8f, 0xe7, 0x48, 0x80, 0xb0, 0x61, 0x96, 0xc9}}, 0xc82329 }; //{13830F54-77A0-4626-8FE7-4880B06196C9} [nvigi.plugin.template.inference.backend.api]}
}
}

const char* kTemplateInputSlotA = "inputSlotA";
const char* kTemplateInputSlotB = "inputSlotB";
const char* kTemplateInputSlotC = "inputSlotC";

//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
//! {78149310-8761-4204-AA00-FB17D8120F0A}
struct alignas(8) TemplateInferCreationParameters {
    TemplateInferCreationParameters() {};
    NVIGI_UID(UID({ 0x78149310, 0x8761, 0x4204,{0xaa, 0x00, 0xfb, 0x17, 0xd8, 0x12, 0x0f, 0x0a} }), kStructVersion1)
    CommonCreationParameters* common;
};

NVIGI_VALIDATE_STRUCT(TemplateInferCreationParameters)

//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
//! {0CB6A547-9880-4A62-A325-8220FD3EDF2F}
struct alignas(8) TemplateInferCreationParametersEx {
    TemplateInferCreationParametersEx() {};
    NVIGI_UID(UID({ 0x0cb6a547, 0x9880, 0x4a62,{0xa3, 0x25, 0x82, 0x20, 0xfd, 0x3e, 0xdf, 0x2f} }), kStructVersion1)
    //! Some extra optional parameters go here
    //! 
    //! This structure is normally chained to TemplateInferCreationParameters using the _next member.
};

NVIGI_VALIDATE_STRUCT(TemplateInferCreationParametersEx)

//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
//! {C1850AC2-AC46-4E4B-A2FA-451B5AC3E905}
struct alignas(8) TemplateInferCapabilitiesAndRequirements {
    TemplateInferCapabilitiesAndRequirements() {};
    NVIGI_UID(UID({ 0xc1850ac2, 0xac46, 0x4e4b,{0xa2, 0xfa, 0x45, 0x1b, 0x5a, 0xc3, 0xe9, 0x05} }), kStructVersion1)
    CommonCapabilitiesAndRequirements* common;
};

NVIGI_VALIDATE_STRUCT(TemplateInferCapabilitiesAndRequirements)

//! Template interface
//! 
//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
//! {7D1DC21C-DD50-4D29-B548-CD099BDE3A97}
struct alignas(8) ITemplateInfer {
    ITemplateInfer() {};
    NVIGI_UID(UID({ 0x7d1dc21c, 0xdd50, 0x4d29,{0xb5, 0x48, 0xcd, 0x09, 0x9b, 0xde, 0x3a, 0x97} }), kStructVersion1)

    //! Creates new instance
    //!
    //! Call this method to create instance for the GPT model
    //! 
    //! @param params Reference to the GPT setup parameters
    //! @param instance Returned new instance (null on error)
    //! @return nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
    //!
    //! This method is NOT thread safe.
    nvigi::Result(*createInstance)(const nvigi::TemplateInferCreationParameters& params, nvigi::InferenceInstance** instance);

    //! Destroys existing instance
    //!
    //! Call this method to destroy an existing GPT instance
    //! 
    //! @param instance Instance to destroy (ok to destroy null instance)
    //! @return nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
    //!
    //! This method is NOT thread safe.
    nvigi::Result(*destroyInstance)(const nvigi::InferenceInstance* instance);

    //! Returns model information
    //!
    //! Call this method to find out about the available models and their capabilities and requirements.
    //! 
    //! @param modelInfo Pointer to structure containing supported model information
    //! @param params Optional pointer to the setup parameters (can be null)
    //! @return nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
    //!
    //! This method is NOT thread safe.
    nvigi::Result (*getCapsAndRequirements)(nvigi::TemplateInferCapabilitiesAndRequirements* modelInfo, const nvigi::TemplateInferCreationParameters* params);

    //! NEW MEMBERS GO HERE, BUMP THE VERSION!    
};

NVIGI_VALIDATE_STRUCT(ITemplateInfer)

}