// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#if NVIGI_WINDOWS
#include <windows.h>
#endif
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <atomic>
#include <cmath>
#include <codecvt>
#include <locale>
#include <iomanip>

#include "source/core/nvigi.api/nvigi_version.h"
#include "external/json/source/nlohmann/json.hpp"
using json = nlohmann::json;

#ifdef NVIGI_WINDOWS
#define NVIGI_IGNOREWARNING_PUSH __pragma(warning(push))
#define NVIGI_IGNOREWARNING_POP __pragma(warning(pop))
#define NVIGI_IGNOREWARNING(w) __pragma(warning(disable : w))
#define NVIGI_IGNOREWARNING_WITH_PUSH(w)                    \
        NVIGI_IGNOREWARNING_PUSH                            \
        NVIGI_IGNOREWARNING(w)
#else
#define NVIGI_IGNOREWARNING_PUSH _Pragma("GCC diagnostic push")
#define NVIGI_IGNOREWARNING_POP _Pragma("GCC diagnostic pop")
#define NVIGI_INTERNAL_IGNOREWARNING(str) _Pragma(#str)
#define NVIGI_IGNOREWARNING(w) NVIGI_INTERNAL_IGNOREWARNING(GCC diagnostic ignored w)
#define NVIGI_IGNOREWARNING_WITH_PUSH(w) NVIGI_IGNOREWARNING_PUSH NVIGI_IGNOREWARNING(w)
#endif


namespace nvigi
{

namespace extra
{

// Ignore deprecated warning, c++17 does not provide proper alternative yet
#ifdef NVIGI_WINDOWS
    NVIGI_IGNOREWARNING_WITH_PUSH(4996)
#endif

inline std::wstring utf8ToUtf16(const char* source)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
    return convert.from_bytes(source);
}

inline std::string utf16ToUtf8(const wchar_t* source)
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
    return convert.to_bytes(source);
}

NVIGI_IGNOREWARNING_POP

inline std::wstring toWStr(const std::string& s)
{
    return utf8ToUtf16(s.c_str());
}

inline std::wstring toWStr(const char* s)
{
    return utf8ToUtf16(s);
}

inline std::string toStr(const std::wstring& s)
{
    return utf16ToUtf8(s.c_str());
}

inline std::string toStr(const wchar_t* s)
{
    return utf16ToUtf8(s);
}

inline std::string toStr(const Version& v)
{
    return std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.build);
}
inline std::wstring toWStr(const Version& v)
{
    return std::to_wstring(v.major) + L"." + std::to_wstring(v.minor) + L"." + std::to_wstring(v.build);
}

inline std::string guidToString(const UID& guid) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    // Output data1, data2, and data3
    ss << std::setw(8) << guid.data1;
    ss << '-';
    ss << std::setw(4) << guid.data2;
    ss << '-';
    ss << std::setw(4) << guid.data3;
    ss << '-';

    // Output data4
    for (size_t i = 0; i < sizeof(guid.data4); ++i) {
        ss << std::setw(2) << static_cast<int>(guid.data4[i]);
    }

    return ss.str();
}

template <typename I>
std::string toHexStr(I w, size_t hex_len = sizeof(I) << 1)
{
    constexpr const char* digits = "0123456789ABCDEF";
    std::string rc(hex_len, '0');
    for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
        rc[i] = digits[(w >> j) & 0x0f];
    return rc;
}

inline constexpr uint32_t align(uint32_t size, uint32_t alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

template <typename T>
inline T getJSONValue(const json& body, const std::string& key, const T& default_value)
{
    // Fallback null to default value
    return body.contains(key) && !body.at(key).is_null() ? body.value(key, default_value) : default_value;
}

//! If value is null it will remove the environment variable
inline bool setEnvVar(const char* varName, const char* value)
{
    bool result;
#if NVIGI_WINDOWS
    result = (SetEnvironmentVariableA(varName, value) != 0);
#else
    if (value)
    {
        result = (setenv(varName, value, /*overwrite=*/1) == 0);
    }
    else
    {
        result = (unsetenv(varName) == 0);
    }
#endif
    return result;
}

inline bool getEnvVar(const char* varName, std::string& value)
{
#if NVIGI_WINDOWS
    std::wstring varNameWide = utf8ToUtf16(varName);

    const size_t staticBufferSize = 256;
    wchar_t staticBuffer[staticBufferSize];
    size_t numCharactersRequired = GetEnvironmentVariableW(varNameWide.c_str(), staticBuffer, staticBufferSize);
    if (numCharactersRequired == 0)
    {
        return false;
    }
    else if (numCharactersRequired > staticBufferSize)
    {
        std::vector<wchar_t> dynamicBufferStorage(numCharactersRequired);
        wchar_t* dynamicBuffer = dynamicBufferStorage.data();

        if (GetEnvironmentVariableW(varNameWide.c_str(), dynamicBuffer, (DWORD)numCharactersRequired) == 0)
        {
            return false;
        }
        value = utf16ToUtf8(dynamicBuffer);
    }
    else
    {
        value = utf16ToUtf8(staticBuffer);
    }
#else
    const char* buffer = getenv(varName);
    if (buffer == nullptr)
    {
        return false;
    }
    value = buffer; // Copy string
#endif
    return true;
}

#if NVIGI_WINDOWS
inline bool getRegistryDword(const WCHAR *InRegKeyHive, const WCHAR *InRegKeyName, DWORD *OutValue)
{
    HKEY Key;
    LONG Res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, InRegKeyHive, 0, KEY_READ, &Key);
    if (Res == ERROR_SUCCESS)
    {
        DWORD dwordSize = sizeof(DWORD);
        Res = RegGetValueW(Key, NULL, InRegKeyName, RRF_RT_REG_DWORD, NULL, (LPBYTE)OutValue, &dwordSize);
        RegCloseKey(Key);
        if (Res == ERROR_SUCCESS)
        {
            return true;
        }
    }
    return false;
};

inline bool getRegistryString(const WCHAR *InRegKeyHive, const WCHAR *InRegKeyName, WCHAR *OutValue, DWORD InCountValue)
{
    HKEY Key;
    LONG Res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, InRegKeyHive, 0, KEY_READ, &Key);
    if (Res == ERROR_SUCCESS)
    {
        DWORD bufferSize = sizeof(WCHAR) * InCountValue;
        Res = RegGetValueW(Key, NULL, InRegKeyName, RRF_RT_REG_SZ, NULL, (LPBYTE)OutValue, &bufferSize);
        RegCloseKey(Key);
        if (Res == ERROR_SUCCESS)
        {
            return true;
        }
    }
    return false;
};
#endif

// Returns a microseconds string as seconds:mseconds:useconds
inline std::string prettifyMicrosecondsString(const uint64_t microseconds)
{
    auto tmp = microseconds;

    // Calculate seconds, milliseconds, and remaining microseconds
    uint64_t seconds = tmp / 1000000;
    tmp %= 1000000;
    uint64_t milliseconds = tmp / 1000;
    tmp %= 1000;

    // Format the result
    std::ostringstream formattedTime;
    formattedTime << seconds << "s:" << std::setw(3) << std::setfill('0') << milliseconds << "ms:" << std::setw(3) << std::setfill('0') << tmp << "us";
    return formattedTime.str();
}

static inline std::chrono::high_resolution_clock::time_point s_timeSinceBegin = std::chrono::high_resolution_clock::now();

// Records a timestamp and returns it as a seconds:mseconds:useconds string
inline std::string getPrettyTimestamp()
{
    std::chrono::duration<uint64_t, std::micro> sinceInit = std::chrono::duration_cast<std::chrono::duration<uint64_t, std::micro>>
                                                                           (std::chrono::high_resolution_clock::now() - s_timeSinceBegin);
    return prettifyMicrosecondsString(sinceInit.count());
}

struct ScopedTasks
{
    ScopedTasks() {};
    ScopedTasks(std::function<void(void)> funIn, std::function<void(void)> funOut) { funIn();  tasks.push_back(funOut); }
    ScopedTasks(std::function<void(void)> fun) { tasks.push_back(fun); }
    void execute()
    {
        for (auto& task : tasks)
        {
            task();
        }
        tasks.clear();
    }
    ~ScopedTasks() { execute(); }
    std::vector<std::function<void(void)>> tasks;
};

namespace keyboard
{
struct VirtKey
{
    VirtKey(int mainKey = 0, bool bShift = false, bool bControl = false, bool bAlt = false)
        : m_mainKey(mainKey)
        , m_bShift(bShift)
        , m_bControl(bControl)
        , m_bAlt(bAlt)
    {}

    std::string asStr() const
    {
        std::string s;
        if (m_bControl) s += "ctrl+";
        if (m_bShift) s += "shift+";
        if (m_bAlt) s += "alt+";
        if (m_mainKey)
        {
            s += (char)m_mainKey;
        }
        else
        {
            s = "unassigned";
        }
        return s;
    }

    // Main key press for the binding
    int m_mainKey = 0;

    // Modifier keys required to match to activate the virtual key binding
    // True means that the corresponding modifier key must be pressed for the virtual key to be considered pressed,
    // and False means that the corresponding modifier key may not be pressed for the virtual key to be considered pressed.
    bool m_bShift = false;
    bool m_bControl = false;
    bool m_bAlt = false;
};

// {E23C9CC6-4EA6-404D-888D-5FD97E0C5FB0}
struct alignas(8) IKeyboard {
    IKeyboard() {}; 
    NVIGI_UID(UID({ 0xe23c9cc6, 0x4ea6, 0x404d,{ 0x88, 0x8d, 0x5f, 0xd9, 0x7e, 0xc, 0x5f, 0xb0 } }), kStructVersion1)
    void (*registerKey)(const char* name, const VirtKey& key);
    bool (*wasKeyPressed)(const char* name);
    const VirtKey& (*getKey)(const char* name);
    bool (*hasFocus)() = 0;

    //! IMPORTANT: New members go here, don't forget to bump the version, see nvigi_struct.h for details
};

IKeyboard* getInterface();
}

constexpr size_t kAverageMeterWindowSize = 120;

//! IMPORTANT: Mainly not thread safe for performance reasons
//! 
//! Only selected "get" methods use atomics.
struct AverageValueMeter
{
    AverageValueMeter()
    {
#ifdef NVIGI_WINDOWS
        QueryPerformanceFrequency(&frequency);
#endif
    };

    AverageValueMeter(const AverageValueMeter& rhs) { operator=(rhs); }

    inline AverageValueMeter& operator=(const AverageValueMeter& rhs)
    {
        n = rhs.n.load();
        val = rhs.val.load();
        sum = rhs.sum;
        memcpy(window, rhs.window, sizeof(double) * kAverageMeterWindowSize);
#ifdef NVIGI_WINDOWS
        frequency = rhs.frequency;
        startTime = rhs.startTime;
        elapsedUs = rhs.elapsedUs;
#endif
        return *this;
    }

    //! NOT thread safe
    void reset()
    {
        n = 0;
        val = 0;
        sum = 0;
        mean = 0;
        memset(window, 0, sizeof(double) * kAverageMeterWindowSize);
#ifdef NVIGI_WINDOWS
        startTime = {};
        elapsedUs = {};
#endif
    }

    //! NOT thread safe
    void begin()
    {
#ifdef NVIGI_WINDOWS
        QueryPerformanceCounter(&startTime);
#endif
    }

    //! NOT thread safe
    void end()
    {
#ifdef NVIGI_WINDOWS
        if (startTime.QuadPart > 0)
        {
            LARGE_INTEGER endTime{};
            QueryPerformanceCounter(&endTime);
            elapsedUs.QuadPart = endTime.QuadPart - startTime.QuadPart;
            elapsedUs.QuadPart *= 1000000;
            elapsedUs.QuadPart /= frequency.QuadPart;
            auto elapsedMs = elapsedUs.QuadPart / 1000.0;
            add(elapsedMs);
        }
#endif
    }

    //! NOT thread safe
    void timestamp()
    {
        end();
        begin();
    }

    //! NOT thread safe
    int64_t timeFromLastTimestampUs()
    {
#ifdef NVIGI_WINDOWS
        if (startTime.QuadPart > 0)
        {
            LARGE_INTEGER endTime{};
            QueryPerformanceCounter(&endTime);
            elapsedUs.QuadPart = endTime.QuadPart - startTime.QuadPart;
            elapsedUs.QuadPart *= 1000000;
            elapsedUs.QuadPart /= frequency.QuadPart;
        }
        return elapsedUs.QuadPart;
#else
        return 0;
#endif
    }

    //! Performance sensitive code, can be called
    //! thousands of times in CPU taxing loops hence
    //! avoiding using std vectors as much as possible.
    //! 
    //! NOT thread safe
    void add(double value)
    {
        val = value;
        sum = sum + value;
        auto i = n.load() % kAverageMeterWindowSize;
        if (n >= kAverageMeterWindowSize)
        {
            sum = sum - window[i];
        }
        window[i] = value;
        n++;
        mean = sum / double(std::min(n.load(), kAverageMeterWindowSize));
    }

    //! NOT thread safe
    double getMedian()
    {
        double median = 0;
        if (n > 0)
        {
            std::vector<double> tmp(window, window + std::min(n.load(),kAverageMeterWindowSize));
            std::sort(tmp.begin(), tmp.end());
            median = tmp[tmp.size() / 2];
        }
        return median;
    }

    //! NOT thread safe
    inline int64_t getElapsedTimeUs() const
    {
#ifdef NVIGI_WINDOWS
        return elapsedUs.QuadPart;
#else
        return 0;
#endif
    }

    //! Thread safe
    //! 
    inline double getMean() const { return mean.load(); }
    inline double getValue() const { return val.load(); }
    inline uint64_t getNumSamples() const { return n.load(); }

private:
    std::atomic<double> val = 0;
    std::atomic<double> mean = 0;
    std::atomic<uint64_t> n = 0;

    double sum{};
    double window[kAverageMeterWindowSize];

#ifdef NVIGI_WINDOWS
    LARGE_INTEGER frequency{};
    LARGE_INTEGER startTime{};
    LARGE_INTEGER elapsedUs{};
#endif
};

struct ScopedCPUTimer
{
    ScopedCPUTimer(AverageValueMeter* meter)
    {
        m_meter = meter;
        meter->begin();
    }
    ~ScopedCPUTimer()
    {
        m_meter->end();
    }

    AverageValueMeter* m_meter{};
};

inline void format(std::ostringstream& stream, const char* str)
{
    stream << str;
}

template <class Arg, class... Args>
inline void format(std::ostringstream& stream, const char* str, Arg&& arg, Args&&... args)
{
    auto p = strstr(str, "{}");
    if (p)
    {
        stream.write(str, p - str);
        auto p1 = strstr(p + 2, "%x");
        if (p1 == p + 2)
        {
            stream << std::hex;
        }
        stream << arg;
        if (p1 == p + 2)
        {
            stream << std::dec;
            p += 2;
        }
        format(stream, p + 2, std::forward<Args>(args)...);
    }
    else
    {
        stream << str;
    }
 }

/**
 * Formats a string similar to the {fmt} library (https://fmt.dev), but header-only and without requiring an external
 * library be included
 *
 * NOTE: This is not intended to be a full replacement for {fmt}. Only '{}' is supported (i.e. no non-positional
 * support). And any type can be formatted, but must be streamable (i.e. have an appropriate operator<<)
 *
 * Example: format("{}, {} and {}: {}", "Peter", "Paul", "Mary", 42) would produce the string "Peter, Paul and Mary: 42"
 * @param str The format string. Use '{}' to indicate where the next parameter would be inserted.
 * @returns The formatted string
 */
template <class... Args>
inline std::string format(const char* str, Args&&... args)
{
    std::ostringstream stream;
    stream.precision(2);
    stream << std::fixed;
    extra::format(stream, str, std::forward<Args>(args)...);
    return stream.str();
}

template <class... Args>
inline std::string formatExt(std::streamsize prec, const char* str, Args&&... args)
{
    std::ostringstream stream;
    stream.precision(prec);
    stream << std::fixed;
    extra::format(stream, str, std::forward<Args>(args)...);
    return stream.str();
}

}
}
