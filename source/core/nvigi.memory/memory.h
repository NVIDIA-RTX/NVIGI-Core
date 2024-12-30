// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "source/core/nvigi.api/nvigi.h"

namespace nvigi
{

namespace memory
{

// {8A6572E0-F713-44C7-A2BF-8493A9499EB2}
struct alignas(8) IMemoryManager {
    IMemoryManager() {}; 
    NVIGI_UID(UID({ 0x8a6572e0, 0xf713, 0x44c7,{ 0xa2, 0xbf, 0x84, 0x93, 0xa9, 0x49, 0x9e, 0xb2 } }), kStructVersion1)
    void* (*allocate)(size_t bytes);
    void (*deallocate)(void* ptr);
#ifdef NVIGI_VALIDATE_MEMORY
    size_t (*getNumAllocations)();
    void (*dumpAllocations)();
#endif
};

NVIGI_VALIDATE_STRUCT(IMemoryManager)

IMemoryManager* getInterface();
}

}