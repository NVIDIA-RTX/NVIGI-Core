// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <iomanip>
#include <map>

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.extra/extra.h"
#include "source/core/nvigi.file/file.h"

namespace nvigi
{
namespace log
{

#ifdef NVIGI_WINDOWS

HMONITOR g_otherMonitor = {};

BOOL MyInfoEnumProc(
    HMONITOR monitor,
    HDC unnamedParam2,
    LPRECT unnamedParam3,
    LPARAM unnamedParam4
)
{
    if (monitor != MonitorFromWindow(GetConsoleWindow(), MONITOR_DEFAULTTONEAREST))
    {
        g_otherMonitor = monitor;
    }
    return TRUE;
}

void moveWindowToAnotherMonitor(HWND hwnd, UINT flags)
{
    RECT prc;
    GetWindowRect(hwnd, &prc);
    
    MONITORINFO mi;
    RECT        rc;
    int         w = 2 * (prc.right - prc.left);
    int         h = 2 * (prc.bottom - prc.top);

    EnumDisplayMonitors(NULL, NULL, MyInfoEnumProc, 0);

    if (g_otherMonitor)
    {
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(g_otherMonitor, &mi);

        //if (flags & MONITOR_WORKAREA)
        rc = mi.rcWork;
        //else
        //    rc = mi.rcMonitor;

        prc.left = rc.left + (rc.right - rc.left - w) / 2;
        prc.top = rc.top + (rc.bottom - rc.top - h) / 2;
        prc.right = prc.left + w;
        prc.bottom = prc.top + h;

        SetWindowPos(hwnd, NULL, prc.left, prc.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

#else

#define _SH_DENYWR 0

NVIGI_IGNOREWARNING_WITH_PUSH("-Wunused-function")
static int _vscprintf (const char * format, va_list pargs) {
      int retval; 
      va_list argcopy; 
      va_copy(argcopy, pargs); 
      retval = vsnprintf(NULL, 0, format, argcopy); 
      va_end(argcopy); 
      return retval; 
}

static FILE* _wfsopen(const wchar_t* wide_filename, const wchar_t* wide_mode, int /*shared_mode*/)
{
    // Convert the wide character filename to a UTF-8 encoded multibyte string
    auto utf8_filename = extra::utf16ToUtf8(wide_filename);
    auto utf8_mode = extra::utf16ToUtf8(wide_mode);    
    return fopen(utf8_filename.c_str(), utf8_mode.c_str());
}

static FILE* _fsopen(const char* utf8_filename, const char* utf8_mode, int /*shared_mode*/)
{
    return fopen(utf8_filename, utf8_mode);
}
NVIGI_IGNOREWARNING_POP
#endif

struct Log
{
    std::hash<std::string> m_hash;
    std::atomic<bool> m_console = false;
    std::atomic<bool> m_pathInvalid = false;
    std::string m_path;
    std::string m_name;
    LogLevel m_logLevel = LogLevel::eVerbose;
    std::atomic<bool> m_consoleActive = false;
    FILE* m_file = {};
    PFun_LogMessageCallback* m_logMessageCallback = {};
    
    Log() {}

    void print(ConsoleForeground color, LogType type, const std::string &logMessage)
    {
#ifdef NVIGI_WINDOWS
        // Set attribute for newly written text
        if (m_consoleActive)
        {
            SetConsoleTextAttribute(m_outHandle, color);
            DWORD OutChars;
            auto logMessageUTF16 = extra::utf8ToUtf16(logMessage.c_str());
            WriteConsoleW(m_outHandle, logMessageUTF16.c_str(), (DWORD)logMessageUTF16.length(), &OutChars, nullptr);
            if (color != nvigi::log::WHITE)
            {
                SetConsoleTextAttribute(m_outHandle, nvigi::log::WHITE);
            }
        }
        else if (!m_logMessageCallback)
        {
            fprintf(stdout, "%s", logMessage.c_str());
            if (type == LogType::eError)
            {
                fprintf(stderr, "%s", logMessage.c_str());
            }
        }

        // Only output to VS debugger if host is not handling it
        if (!m_logMessageCallback)
        {
            OutputDebugStringA(logMessage.c_str());
        }
#else
        (void)color;
        fprintf(stdout, "%s", logMessage.c_str());
        if (type == LogType::eError)
        {
            fprintf(stderr, "%s", logMessage.c_str());
        }
#endif

        if (m_file)
        {
            fputs(logMessage.c_str(), m_file);
            fflush(m_file);
        }
    }

    void startConsole()
    {
#ifdef NVIGI_WINDOWS
        if (!isConsoleActive() || !m_outHandle)
        {
            AllocConsole();
            SetConsoleTitleA("NVIGI");
            moveWindowToAnotherMonitor(GetConsoleWindow(), 0);
            m_outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        }
#endif
    }

    bool isConsoleActive()
    {
#ifdef NVIGI_WINDOWS
        HWND consoleWnd = GetConsoleWindow();
        return consoleWnd != NULL;
#else
        return true;
#endif
    }
    
    float m_messageDelayMs = 5000.0f;

    inline static Log* s_log = {};
    inline static ILog s_ilog = {};
#ifdef NVIGI_WINDOWS
    HANDLE m_outHandle{};
#endif
};

void enableConsole(bool flag)
{
    auto& ctx = *Log::s_log;
    ctx.m_console = flag;
}

LogLevel getLogLevel()
{
    auto& ctx = *Log::s_log;
    return ctx.m_logLevel;
}

void setLogLevel(LogLevel level)
{
    auto& ctx = *Log::s_log;
    ctx.m_logLevel = level;
}

const char* getLogPath()
{ 
    auto& ctx = *Log::s_log;
    return ctx.m_path.c_str(); 
}

void setLogPath(const char* _path)
{
    auto& ctx = *Log::s_log;
    std::u8string path;
    // Handle long paths for logging as needed on Windows platform
    if(_path)
    {
        if (!nvigi::file::getOSValidPath((const char8_t*)_path, path)) 
        {
            // In the middle of setting up logging so just try to print to console or callback
            ctx.print(RED, LogType::eError, "Provided path to the log file is invalid");
            return;
        }
        if (!fs::is_directory(path))
        {
            ctx.print(RED, LogType::eError, "Provided path to the log file is not a directory");
            return;
        }
        auto p1 = fs::path(path);
        if (ctx.m_file)
        {
            // Do not reset log file if paths are pointing to the same location
            auto p2 = fs::path(ctx.m_path);
            if (p1 != p2)
            {
                fflush(ctx.m_file);
                fclose(ctx.m_file);
                ctx.m_file = nullptr;
            }
        }
    }
    // Passing nullptr will disable logging to a file
    ctx.m_path = _path ? (const char*)path.c_str() : "";
    ctx.m_pathInvalid = false;
}

void setLogName(const char* name)
{
    auto& ctx = *Log::s_log;
    ctx.m_name = name;
}

const char* getLogName()
{ 
    auto& ctx = *Log::s_log;
    return ctx.m_name.c_str();
}

void setLogCallback(void* logMessageCallback)
{
    auto& ctx = *Log::s_log;
    ctx.m_logMessageCallback = (PFun_LogMessageCallback*)logMessageCallback;
}

void setLogMessageDelay(float messageDelayMs)
{
    auto& ctx = *Log::s_log;
    ctx.m_messageDelayMs = messageDelayMs;
}

void shutdown()
{
    auto& ctx = *Log::s_log;
    if (ctx.m_file)
    {
        fflush(ctx.m_file);
        fclose(ctx.m_file);
        ctx.m_file = nullptr;
        ctx.m_pathInvalid = true; // prevent log file reopening
    }
    ctx.m_consoleActive = false;
#ifdef NVIGI_WINDOWS
    // Win32 API does not require us to close this handle
    ctx.m_outHandle = {};
#endif
}

std::string getCurrentDateTime() {
    // Get current time as time_point
    auto now = std::chrono::system_clock::now();
    auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // Convert to tm struct
    std::tm nowTm = *std::localtime(&nowAsTimeT);

    // Format date and time
    std::ostringstream dateTimeStream;
    dateTimeStream << std::put_time(&nowTm, "%Y-%m-%d %H:%M:%S");
    dateTimeStream << '.' << std::setfill('0') << std::setw(3) << nowMs.count();

    return dateTimeStream.str();
}

void logva(uint32_t level, ConsoleForeground color, const char* _file, int line, const char* _func, int type, const char* tag, const char* _fmt, ...)
{
    // NOTE: This method must be thread safe but for performance we do NOT use any heavy synchronization
    auto& ctx = Log::s_log;

    auto generateHeader = [](const char* fl, int l, const char* fn, int t, const char* customTag)->std::string
        {
            try
            {
                // Filename
                std::string f(fl);
                // file is constexpr so always valid and always will have at least one '\'
                f = f.substr(f.rfind('\\') + 1);

                // Log type
                static std::string prefix[] = { "info","warn","error" };
                static_assert(countof(prefix) == (size_t)LogType::eCount);

                // Put it all together in the message header
                std::ostringstream oss;
                oss << "[" << getCurrentDateTime() << "]" << "[nvigi][" << prefix[t] << "]";
                if (customTag)
                {
                    oss << "[" << customTag << "]";
                }
                oss << "[" << f << ":" << l << "]" << "[" << fn << "]";
                return oss.str();
            }
            catch (const std::exception&)
            {
                return std::string(customTag);
            }
        };

    try
    {
        if (level > (uint32_t)ctx->m_logLevel)
        {
            // Higher level than requested, bail out
            return;
        }

        std::string message;

        bool errorDetected = false;
        //! Important, va_list cannot be used multiple times!
        va_list args, args1;

        // Make sure va_end is called before early out!
        va_start(args, _fmt);
        va_copy(args1, args);

        // Determine the required size of the formatted message (without null-terminator)
        int msgSize = std::vsnprintf(nullptr, 0, _fmt, args); //_vscprintf(_fmt, args);
        if (msgSize >= 1)
        {
            // Account for the null terminator char
            msgSize++;
            message.resize(msgSize);

            // Format the message
            msgSize = std::vsnprintf(message.data(), message.size(), _fmt, args1);

            if (msgSize <= 0)
            {
                // invalid character in the string or any other error
                errorDetected = true;
            }
            // vsnprintf adds '0' at the end of the string. if we don't remove it,
            // the '+=' won't work correctly on that string
            while (message.size() && *message.rbegin() == '\0')
            {
                message.pop_back();
            }
        }
        else
        {
            // _fmt is bad or empty log
            errorDetected = true;
        }
        va_end(args);
        va_end(args1);

        if (errorDetected)
        {
            // Something went wrong during `_vscprintf` or `vsprintf_s`
            return;
        }

        // Atomics
        if (ctx->m_console.load() && !ctx->m_consoleActive.load())
        {
            ctx->startConsole();
            ctx->m_consoleActive = ctx->isConsoleActive();
        }

        if (!ctx->m_file && !ctx->m_path.empty() && !ctx->m_pathInvalid)
        {
            // Allow other process to read log file
            auto path = ctx->m_path + "\\" + ctx->m_name;
            auto oldLocale = setlocale(LC_ALL, ".65001"); // utf-8
            ctx->m_file = _fsopen(path.c_str(), "wt", _SH_DENYWR);
            setlocale(LC_ALL, oldLocale);
            if (!ctx->m_file)
            {
                ctx->m_pathInvalid = true;
                std::string completeLogMessageLocal = generateHeader(__FILE__, __LINE__, __func__, (int)LogType::eError, nullptr) + "Failed to open log file " + path + "\n";
                ctx->print(RED, LogType::eError, completeLogMessageLocal);
            }
            else
            {
                std::string completeLogMessageLocal = generateHeader(__FILE__, __LINE__, __func__, (int)LogType::eInfo, nullptr) + "Log file " + path + " opened on " + getCurrentDateTime() + "\n";
                ctx->print(WHITE, LogType::eInfo, completeLogMessageLocal);
            }
        }

        auto completeLogMessage = generateHeader(_file, line, _func, type, tag) + message;
        if (completeLogMessage.back() != '\n')
        {
            completeLogMessage += '\n';
        }
        if (ctx->m_logMessageCallback)
        {
            ctx->m_logMessageCallback((LogType)type, completeLogMessage.c_str());
        }
        ctx->print(color, (LogType)type, completeLogMessage);
    }
    catch (const std::exception& err) 
    {
        std::string generalLogWarnMessage = generateHeader(__FILE__, __LINE__, __func__, (int)LogType::eWarn, nullptr) + "NVIGI Logger generated exception: " + std::string(err.what()) + "\n";
        ctx->print(DARKYELLOW, LogType::eWarn, generalLogWarnMessage);
    }
}

ILog* getInterface()
{
    if (!Log::s_log)
    {
        Log::s_log = new Log();
        Log::s_ilog.logva = logva;
        Log::s_ilog.enableConsole = enableConsole;
        Log::s_ilog.getLogLevel = getLogLevel;
        Log::s_ilog.setLogLevel = setLogLevel;
        Log::s_ilog.setLogPath = setLogPath;
        Log::s_ilog.setLogName = setLogName;
        Log::s_ilog.setLogCallback = setLogCallback;
        Log::s_ilog.setLogMessageDelay = setLogMessageDelay;
        Log::s_ilog.getLogPath = getLogPath;
        Log::s_ilog.getLogName = getLogName;
        Log::s_ilog.shutdown = shutdown;
    }
    return &Log::s_ilog;
}

void destroyInterface()
{
    if (Log::s_log)
    {
        Log::s_ilog.shutdown();
        delete Log::s_log;
        Log::s_log = {};
    }
}

}
}
