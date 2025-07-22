// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <string>
#include <vector>
#ifdef NVIGI_WINDOWS
#include <windows.h>
#else
// keep gcc happy with forward declarations
#endif

#define NVIGI_VALIDATE(f) {auto r = f; if(r != nvigi::kResultOk) { NVIGI_LOG_ERROR("%s failed - error code 0x%x", #f, r); } };

#ifdef NVIGI_WINDOWS

#elif NVIGI_LINUX
#include <unistd.h>
#include <dlfcn.h>
using HMODULE = void*;
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS    0x00001000
#define GetProcAddress dlsym
#define FreeLibrary dlclose
#define LoadLibraryA(lib) dlopen(lib, RTLD_LAZY | RTLD_LOCAL)
#define LoadLibraryW(lib) dlopen(nvigi::extra::toStr(lib).c_str(), RTLD_LAZY | RTLD_LOCAL)
#define LoadLibraryExW(lib,reserved,flags) dlopen(nvigi::extra::toStr(lib).c_str(), RTLD_LAZY | RTLD_LOCAL)

#define sscanf_s sscanf
#define strcpy_s(a,b,c) strcpy(a,c)
#define strcat_s(a,b,c) strcat(a,c)
#define memcpy_s(a,b,c,d) memcpy(a,c,d)
typedef struct __LUID
{
    unsigned long LowPart;
    long HighPart;
} 	LUID;

#endif

namespace nvigi
{
template<typename T>
bool isInterfaceSameOrNewerVersion(T* _interface)
{
    T tmp{};
    return _interface->getVersion() >= tmp.getVersion();
}
}
