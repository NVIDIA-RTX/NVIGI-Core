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
namespace tmpl
{
    constexpr PluginID kIdBackendApi = { {0x5d06990d, 0x62f6, 0x40bf,{0x80, 0x07, 0x33, 0x8e, 0x83, 0x2f, 0x4c, 0xec}}, 0xeee3d7 }; //{5D06990D-62F6-40BF-8007-338E832F4CEC} [nvigi.plugin.tmpl.backend.api]
}
}

//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
// {506C5935-67C6-4136-9550-36BBA83C93BC}
struct alignas(8) TemplateParameters {
    TemplateParameters() {}; 
    NVIGI_UID(UID({ 0x506c5935, 0x67c6, 0x4136,{ 0x95, 0x50, 0x36, 0xbb, 0xa8, 0x3c, 0x93, 0xbc } }), kStructVersion1)
    //! Add your parameters here
};

NVIGI_VALIDATE_STRUCT(TemplateParameters)

//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
// {516C5935-67C6-4136-9550-36BBA83C93BC}
struct alignas(8) TemplateParametersEx {
    TemplateParametersEx() {}; 
    NVIGI_UID(UID({ 0x516c5935, 0x67c6, 0x4136,{ 0x95, 0x50, 0x36, 0xbb, 0xa8, 0x3c, 0x93, 0xbc } }), kStructVersion1)
    //! Some extra optional parameters go here
    //! 
    //! This structure is normally chained to TemplateParameters using the _next member but it can be used standalone as well
};

NVIGI_VALIDATE_STRUCT(TemplateParametersEx)

//! Template interface
//! 
//! IMPORTANT: DO NOT DUPLICATE GUIDs - WHEN CLONING AND REUSING STRUCTURES ALWAYS ASSIGN NEW GUID
//! 
//! Run .\tools\nvigi.tool.utils.exe --interface MyInterfaceName and paste new structs here, delete below templates as needed
//! 
// {6D981FF9-B73A-443E-B0A2-71FF70B08CCE}
struct alignas(8) ITemplate {
    ITemplate() {}; 
    NVIGI_UID(UID({ 0x6d981ff9, 0xb73a, 0x443e,{ 0xb0, 0xa2, 0x71, 0xff, 0x70, 0xb0, 0x8c, 0xce } }), kStructVersion1)

    //! My custom function A
    //!
    //! Call this method to do task A
    //! 
    //! @param paramX Parameter X
    //! @param paramY Parameter Y
    //! @param ... Parameter ...
    //! @return nvigi::kResultOk if successful, error code otherwise (see nvigi_result.h for details)
    //!
    //! This method is NOT thread safe.
    nvigi::Result(*funA)(...);
    
    //! and so on, add more functions etc.
    
    //! NEW MEMBERS GO HERE, BUMP THE VERSION!    
};

NVIGI_VALIDATE_STRUCT(ITemplate)

}