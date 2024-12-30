// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#ifdef NVIGI_WINDOWS
#include <windows.h>

#include "source/core/nvigi.extra/extra.h"

#define NVIGI_ENABLE_EXCEPTION_HANDLING

#ifdef NVIGI_ENABLE_EXCEPTION_HANDLING
#define NVIGI_CATCH_EXCEPTION(FUN)                                                                           \
__try                                                                                                       \
{                                                                                                           \
    return FUN;                                                                                             \
}                                                                                                           \
__except (nvigi::exception::getInterface()->writeMiniDump(GetExceptionInformation()))                        \
{ return nvigi::kResultException; }

#define NVIGI_CATCH_EXCEPTION_NO_RETURN(FUN)                                                                \
__try                                                                                                       \
{                                                                                                           \
    FUN;                                                                                                    \
}                                                                                                           \
__except (nvigi::exception::getInterface()->writeMiniDump(GetExceptionInformation()))                        \
{}

#define NVIGI_CATCH_EXCEPTION_RETURN(FUN, RET)                                                               \
__try                                                                                                       \
{                                                                                                           \
    return FUN;                                                                                             \
}                                                                                                           \
__except (nvigi::exception::getInterface()->writeMiniDump(GetExceptionInformation()))                        \
{ return RET; }

#else
#define NVIGI_CATCH_EXCEPTION(FUN) return FUN;
#define NVIGI_CATCH_EXCEPTION_RETURN(FUN, RET) return FUN;
#endif

#else // NVIGI_LINUX

// TODO for Linux
#define NVIGI_CATCH_EXCEPTION(FUN) return FUN;
#define NVIGI_CATCH_EXCEPTION_RETURN(FUN, RET) return FUN;
#define NVIGI_CATCH_EXCEPTION_NO_RETURN(FUN, RET) FUN;

#endif // NVIGI_WINDOWS

namespace nvigi
{
namespace exception
{

// {75E7A7BB-CA10-45A8-966D-B99000D6EA35}
struct alignas(8) IException {
    IException() {}; 
    NVIGI_UID(UID({ 0x75e7a7bb, 0xca10, 0x45a8,{ 0x96, 0x6d, 0xb9, 0x90, 0x0, 0xd6, 0xea, 0x35 } }), kStructVersion1)
    bool (*addExceptionHandler)(void);
    bool (*removeExceptionHandler)(void);
#ifdef NVIGI_WINDOWS
    int (*writeMiniDump)(LPEXCEPTION_POINTERS exceptionInfo);
    void (*setMiniDumpLocation)(const char* miniDumpDirectoryPath);
#endif
};

NVIGI_VALIDATE_STRUCT(IException)

IException* getInterface();
void destroyInterface();

}
}