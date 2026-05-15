// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <string>
#include <vector>
#ifdef NVIGI_WINDOWS
#include <windows.h>
#endif

#define NVIGI_VALIDATE(f) {auto r = f; if(r != nvigi::kResultOk) { NVIGI_LOG_ERROR("%s failed - error code 0x%x", #f, r); } };

namespace nvigi
{
template<typename T>
bool isInterfaceSameOrNewerVersion(T* _interface)
{
    T tmp{};
    return _interface->getVersion() >= tmp.getVersion();
}
}
