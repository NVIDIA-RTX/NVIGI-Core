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
namespace tmpl_infer_cuda
{
    constexpr PluginID kId = { {0x96ff9872, 0x7007, 0x4f1a,{0x9f, 0xa4, 0xe3, 0xa8, 0x66, 0xcf, 0x2a, 0xb1}}, 0x9f44d8 }; //{96FF9872-7007-4F1A-9FA4-E3A866CF2AB1} [nvigi.plugin.template_infer.inference.cuda]
}

}

//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
//! {67A0ECF3-6B52-47F5-B593-8EF877601224}
struct alignas(8) TemplateInferCudaCreationParameters
{
    TemplateInferCudaCreationParameters() { };
    NVIGI_UID(UID({ 0x67a0ecf3, 0x6b52, 0x47f5,{0xb5, 0x93, 0x8e, 0xf8, 0x77, 0x60, 0x12, 0x24} }), kStructVersion1)
    CommonCreationParameters* common;

    //! v1 members go here, please do NOT break the C ABI compatibility:

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvigi::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(TemplateInferCudaCreationParameters)


//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
//! {004C8BF8-E3E1-4C6F-855F-548F91B26CC3}
struct alignas(8) TemplateInferCudaCreationParametersEx
{
    TemplateInferCudaCreationParametersEx() { };
    NVIGI_UID(UID({ 0x004c8bf8, 0xe3e1, 0x4c6f,{0x85, 0x5f, 0x54, 0x8f, 0x91, 0xb2, 0x6c, 0xc3} }), kStructVersion1)

    //! v1 members go here, please do NOT break the C ABI compatibility:

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvigi::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(TemplateInferCudaCreationParametersEx)


//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
//! {3BB81893-49FE-4D27-9979-F7608B9A5A73}
struct alignas(8) TemplateInferCudaCapabilitiesAndRequirements
{
    TemplateInferCudaCapabilitiesAndRequirements() { };
    NVIGI_UID(UID({ 0x3bb81893, 0x49fe, 0x4d27,{0x99, 0x79, 0xf7, 0x60, 0x8b, 0x9a, 0x5a, 0x73} }), kStructVersion1)

    //! v1 members go here, please do NOT break the C ABI compatibility:

    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality
    //! * do not use nested structures, always use pointer members
    //! * do not use internal types in _public_ interfaces (like for example 'nvigi::types::vector' etc.)
    //! * do not change or move any existing members once interface has shipped

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(TemplateInferCudaCapabilitiesAndRequirements)


//! Template interface
//! 
//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
//! {D0876F12-92F5-4F18-B812-B95DBABD38D8}
struct alignas(8) ITemplateInferCuda
{
    ITemplateInferCuda() { };
    NVIGI_UID(UID({ 0xd0876f12, 0x92f5, 0x4f18,{0xb8, 0x12, 0xb9, 0x5d, 0xba, 0xbd, 0x38, 0xd8} }), kStructVersion1)

    //! Creates new instance
    //!
    //! Call this method to create instance for the GPT model
    //! 
    //! @param params Reference to the GPT setup parameters
    //! @param instance Returned new instance (null on error)
    //! @return nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
    //!
    //! This method is NOT thread safe.
    nvigi::Result(*createInstance)(const nvigi::TemplateInferCudaCreationParameters& params, nvigi::InferenceInstance** instance);

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
    nvigi::Result (*getCapsAndRequirements)(nvigi::TemplateInferCudaCapabilitiesAndRequirements* modelInfo, const nvigi::TemplateInferCudaCreationParameters* params);

    //! NEW MEMBERS GO HERE, BUMP THE VERSION!    
};

NVIGI_VALIDATE_STRUCT(ITemplateInferCuda)

}