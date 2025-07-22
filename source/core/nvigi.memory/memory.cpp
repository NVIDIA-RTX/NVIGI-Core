// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#ifdef NVIGI_WINDOWS
#include <Windows.h>
#else
#include <string.h>
#endif

#ifdef NVIGI_VALIDATE_MEMORY
#include <mutex>
#include <map>
#include <assert.h>
#endif

#include "source/core/nvigi.memory/memory.h"
#include "source/core/nvigi.log/log.h"

namespace nvigi
{
namespace memory
{

#ifdef NVIGI_VALIDATE_MEMORY
std::mutex mtx;
std::map<void*, size_t> allocs;
#endif

void* allocate(size_t size)
{
    //NVIGI_LOG_HINT("allocate %llu", size);
    if (!size) return nullptr;
    auto ptr = malloc(size);
    memset(ptr, 0, size);
#ifdef NVIGI_VALIDATE_MEMORY
    std::scoped_lock lock(mtx);
    assert(allocs.find(ptr) == allocs.end());
    allocs[ptr] = size;
#endif
    return ptr;
}

void deallocate(void* ptr)
{
    if (!ptr) return;
    //NVIGI_LOG_HINT("deallocate 0x%llx", ptr);
#ifdef NVIGI_VALIDATE_MEMORY
    std::scoped_lock lock(mtx);
    assert(allocs.find(ptr) != allocs.end());
    allocs.erase(ptr);
#endif
    free(ptr);
}

#ifdef NVIGI_VALIDATE_MEMORY
size_t getNumAllocations() 
{
    std::scoped_lock lock(mtx);
    return allocs.size(); 
}

void dumpAllocations()
{
    std::scoped_lock lock(mtx);
    printf("Remaining allocations:\n");
    for (auto [ptr, sz] : allocs)
    {
        printf("%p size %llu\n", (void*)ptr, (long long unsigned int)sz);
    }
}
#endif

IMemoryManager s_mm{};
IMemoryManager* getInterface() 
{ 
    if (!s_mm.allocate)
    {
        s_mm.allocate = allocate;
        s_mm.deallocate = deallocate;
#ifdef NVIGI_VALIDATE_MEMORY
        s_mm.getNumAllocations = getNumAllocations;
        s_mm.dumpAllocations = dumpAllocations;
#endif
    }
    return &s_mm; 
}

}
}
