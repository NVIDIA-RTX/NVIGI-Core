// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#ifdef NVIGI_WINDOWS
#include <windows.h>
#else
#include <stdarg.h>
#endif
#include <atomic>
#include <chrono>

#include "source/core/nvigi.api/nvigi_struct.h"

template <typename T, size_t N>
constexpr size_t countof(T const (&)[N]) noexcept
{
    return N;
}

namespace nvigi
{

enum class LogLevel : uint32_t;
enum class LogType : uint32_t;

namespace log
{

#ifndef NVIGI_WINDOWS
#define FOREGROUND_BLUE 1
#define FOREGROUND_GREEN 2
#define FOREGROUND_RED 4
#define FOREGROUND_INTENSITY 8
#endif

enum ConsoleForeground
{
    BLACK = 0,
    DARKBLUE = FOREGROUND_BLUE,
    DARKGREEN = FOREGROUND_GREEN,
    DARKCYAN = FOREGROUND_GREEN | FOREGROUND_BLUE,
    DARKRED = FOREGROUND_RED,
    DARKMAGENTA = FOREGROUND_RED | FOREGROUND_BLUE,
    DARKYELLOW = FOREGROUND_RED | FOREGROUND_GREEN,
    DARKGRAY = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
    GRAY = FOREGROUND_INTENSITY,
    BLUE = FOREGROUND_INTENSITY | FOREGROUND_BLUE,
    GREEN = FOREGROUND_INTENSITY | FOREGROUND_GREEN,
    CYAN = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,
    RED = FOREGROUND_INTENSITY | FOREGROUND_RED,
    MAGENTA = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,
    YELLOW = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,
    WHITE = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
};

// {8FFD0CA2-62A0-4F4A-8840-E27E3FF4F75F}
struct alignas(8) ILog {
    ILog() {}; 
    NVIGI_UID(UID({ 0x8ffd0ca2, 0x62a0, 0x4f4a,{0x88, 0x40, 0xe2, 0x7e, 0x3f, 0xf4, 0xf7, 0x5f} }), kStructVersion1)
    void (*logva)(uint32_t level, ConsoleForeground color, const char *file, int line, const char *func, int type, const char* tag, const char *fmt,...);
    void (*enableConsole)(bool flag);
    LogLevel(*getLogLevel)();
    void (*setLogLevel)(LogLevel level);
    void (*setLogPath)(const char *path);
    void (*setLogName)(const char *name);
    void (*setLogCallback)(void* logMessageCallback);
    void (*setLogMessageDelay)(float logMessageDelayMS);
    const char* (*getLogPath)();
    const char* (*getLogName)();
    void (*shutdown)();

    //! IMPORTANT: New members go here, don't forget to bump the version, see nvigi_struct.h for details
};

NVIGI_VALIDATE_STRUCT(ILog)

ILog* getInterface();
void destroyInterface();

#define NVIGI_RUN_ONCE                                  \
    for (static std::atomic<int> s_runAlready(false); \
         !s_runAlready.fetch_or(true);)               \

#define NVIGI_LOG(tag, type, clr, fmt,...) nvigi::log::getInterface()->logva(2, clr, __FILE__,__LINE__,__func__, (int)type, tag, fmt,##__VA_ARGS__)
#define NVIGI_LOG_HINT(fmt,...) nvigi::log::getInterface()->logva(2, nvigi::log::CYAN, __FILE__,__LINE__,__func__, 0, nullptr,fmt,##__VA_ARGS__)
#define NVIGI_LOG_INFO(fmt,...) nvigi::log::getInterface()->logva(1, nvigi::log::WHITE, __FILE__,__LINE__,__func__, 0, nullptr,fmt,##__VA_ARGS__)
#define NVIGI_LOG_WARN(fmt,...) nvigi::log::getInterface()->logva(1, nvigi::log::YELLOW, __FILE__,__LINE__,__func__, 1, nullptr,fmt,##__VA_ARGS__)
#define NVIGI_LOG_ERROR(fmt,...) nvigi::log::getInterface()->logva(1, nvigi::log::RED, __FILE__,__LINE__,__func__, 2, nullptr,fmt,##__VA_ARGS__)
#define NVIGI_LOG_VERBOSE(fmt,...) nvigi::log::getInterface()->logva(2, nvigi::log::WHITE, __FILE__,__LINE__,__func__,0, nullptr,fmt,##__VA_ARGS__)

//! Used by unit test, same as regular logging but with [test] tag and showing in GREEN color in terminal for clear separation
#define NVIGI_LOG_TEST_INFO(fmt,...) nvigi::log::getInterface()->logva(1, nvigi::log::GREEN, __FILE__,__LINE__,__func__, 0, "test",fmt,##__VA_ARGS__)
#define NVIGI_LOG_TEST_WARN(fmt,...) nvigi::log::getInterface()->logva(1, nvigi::log::YELLOW, __FILE__,__LINE__,__func__, 1, "test",fmt,##__VA_ARGS__)
#define NVIGI_LOG_TEST_ERROR(fmt,...) nvigi::log::getInterface()->logva(1, nvigi::log::RED, __FILE__,__LINE__,__func__, 2, "test",fmt,##__VA_ARGS__)
#define NVIGI_LOG_TEST_VERBOSE(fmt,...) nvigi::log::getInterface()->logva(2, nvigi::log::GREEN, __FILE__,__LINE__,__func__,0, "test",fmt,##__VA_ARGS__)

#define NVIGI_LOG_HINT_ONCE(fmt,...) NVIGI_RUN_ONCE { NVIGI_LOG_HINT(fmt,##__VA_ARGS__); }
#define NVIGI_LOG_INFO_ONCE(fmt,...) NVIGI_RUN_ONCE { NVIGI_LOG_INFO(fmt,##__VA_ARGS__); }
#define NVIGI_LOG_WARN_ONCE(fmt,...) NVIGI_RUN_ONCE { NVIGI_LOG_WARN(fmt,##__VA_ARGS__); }
#define NVIGI_LOG_ERROR_ONCE(fmt,...) NVIGI_RUN_ONCE { NVIGI_LOG_ERROR(fmt,##__VA_ARGS__); }
#define NVIGI_LOG_VERBOSE_ONCE(fmt,...) NVIGI_RUN_ONCE { NVIGI_LOG_VERBOSE(fmt,##__VA_ARGS__); }

}
}

