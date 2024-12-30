// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

namespace nvigi
{ 

template<typename T>
struct RuntimeContextScope
{
    RuntimeContextScope(T& _ctx) : ctx(_ctx)
    {
        ctx.cudaContext.pushRuntimeContext();
    }
    ~RuntimeContextScope()
    {
        ctx.cudaContext.popRuntimeContext();
    }
    T& ctx;
};

}