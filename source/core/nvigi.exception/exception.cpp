// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.exception/exception.h"
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.extra/extra.h"
#include "_artifacts/gitVersion.h"

#ifdef NVIGI_WINDOWS

#include <ShlObj.h>
#include <Dbghelp.h.>

//! NOTE: Some code segments taken from https://www.debuginfo.com/

namespace nvigi
{
namespace exception
{

#define CALL_FIRST 1
#define CALL_LAST 0

//! Exception handler
//! 
struct ExceptionContext
{
    std::mutex mtx;
    std::vector<PVOID> exceptionHandlers{};
    IException iexception{};
    fs::path miniDumpDirectory{};
};
static ExceptionContext* ctx{};

constexpr size_t kMaxAllowedDumps = 5;

void deleteOldestDirectoryIfExceeds(const fs::path& target_path, std::size_t max_directories) {
    try {
        // Convert the target path to an absolute path (resolve relative paths)
        fs::path absolute_path = fs::absolute(target_path);

        // Vector to store directories and their last write time
        std::vector<std::pair<fs::path, std::chrono::file_clock::time_point>> directories;

        // Scan the target path for directories
        for (const auto& entry : fs::directory_iterator(absolute_path)) {
            if (entry.is_directory()) {
                auto last_write_time = fs::last_write_time(entry);
                directories.emplace_back(entry.path(), last_write_time);
            }
        }

        // Check if the number of directories exceeds the max limit
        if (directories.size() >= max_directories) {
            // Sort directories by last write time (oldest first)
            std::sort(directories.begin(), directories.end(),
                [](const auto& a, const auto& b) {
                    return a.second < b.second; // Sort by time point (oldest first)
                });

            // Get the oldest directory
            const auto& oldest_directory = directories.front().first;

            // Recursively remove the oldest directory
            fs::remove_all(oldest_directory);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        NVIGI_LOG_ERROR("Filesystem error: %s", e.what());
    } catch (const std::exception& e) {
        NVIGI_LOG_ERROR("Error: %s", e.what());
    }
}

bool dumpStackTrace(EXCEPTION_POINTERS* ExceptionInfo, std::string& stackTrace)
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    CONTEXT* context = ExceptionInfo->ContextRecord;

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));

    bool nvigiException = false;

#ifdef _M_IX86
    stackFrame.AddrPC.Offset = context->Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    stackFrame.AddrPC.Offset = context->Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
#error "Platform not supported!"
#endif

    while (StackWalk64(
#ifdef _M_IX86
        IMAGE_FILE_MACHINE_I386,
#elif _M_X64
        IMAGE_FILE_MACHINE_AMD64,
#else
#error "Platform not supported!"
#endif
        process,
        thread,
        &stackFrame,
        context,
        NULL,
        SymFunctionTableAccess64,
        SymGetModuleBase64,
        NULL))
    {

        // Here you'd use the stack.AddrPC.Offset as the program counter address.
        // To get more meaningful information like function names or source line numbers,
        // you would use SymFromAddr and related functions.
        DWORD64 address = stackFrame.AddrPC.Offset;
        // Retrieve the module (DLL/EXE) name
        IMAGEHLP_MODULE64 moduleInfo = {};
        moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
        if (SymGetModuleInfo64(process, address, &moduleInfo))
        {
            // Example to get the symbol name
            BYTE symbolBuffer[sizeof(SYMBOL_INFO) + 256] = { 0 };
            PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuffer;
            pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            pSymbol->MaxNameLen = 255;
            if (SymFromAddr(process, address, 0, pSymbol))
            {
                nvigiException |= std::string(moduleInfo.ModuleName).find("nvigi.") != std::string::npos;
                stackTrace += extra::format("{}:{}:{}%x\n",moduleInfo.ModuleName, pSymbol->Name, address);
            }
        }
    }
    return nvigiException;
}

///////////////////////////////////////////////////////////////////////////////
// This function determines whether we need data sections of the given module 
//

bool isDataSectionNeeded(const WCHAR* pModuleName)
{
    // Check parameters 

    if (pModuleName == 0)
    {
        assert("Parameter is null.");
        return false;
    }


    // Extract the module name 

    WCHAR szFileName[_MAX_FNAME] = L"";

    _wsplitpath(pModuleName, NULL, NULL, szFileName, NULL);


    // Compare the name with the list of known names and decide 
    if (wcsstr(szFileName, L"nvigi.") != nullptr)
    {
        return true;
    }
    else if (_wcsicmp(szFileName, L"ntdll") == 0)
    {
        return true;
    }


    // Complete 

    return false;

}
///////////////////////////////////////////////////////////////////////////////
// Custom minidump callback 
//

BOOL CALLBACK nvigiMiniDumpCallback(
    PVOID                            pParam,
    const PMINIDUMP_CALLBACK_INPUT   pInput,
    PMINIDUMP_CALLBACK_OUTPUT        pOutput
)
{
    BOOL bRet = FALSE;


    // Check parameters 

    if (pInput == 0)
        return FALSE;

    if (pOutput == 0)
        return FALSE;


    // Process the callbacks 

    switch (pInput->CallbackType)
    {
    case IncludeModuleCallback:
    {
        // Include the module into the dump 
        bRet = TRUE;
    }
    break;

    case IncludeThreadCallback:
    {
        // Include the thread into the dump 
        bRet = TRUE;
    }
    break;

    case ModuleCallback:
    {
        // Are data sections available for this module ? 

        if (pOutput->ModuleWriteFlags & ModuleWriteDataSeg)
        {
            // Yes, they are, but do we need them? 

            if (!isDataSectionNeeded(pInput->Module.FullPath))
            {
                pOutput->ModuleWriteFlags &= (~ModuleWriteDataSeg);
            }
        }

        bRet = TRUE;
    }
    break;

    case ThreadCallback:
    {
        // Include all thread information into the minidump 
        bRet = TRUE;
    }
    break;

    case ThreadExCallback:
    {
        // Include this information 
        bRet = TRUE;
    }
    break;

    case MemoryCallback:
    {
        // We do not include any information here -> return FALSE 
        bRet = FALSE;
    }
    break;

    case CancelCallback:
        break;
    }

    return bRet;

}


int writeMiniDump(LPEXCEPTION_POINTERS exceptionInfo)
{
#ifndef NVIGI_PRODUCTION
    if (IsDebuggerPresent())
    {
        __debugbreak();
    };
#endif

    // Unique id
    auto id = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    std::wstring path = L"";
    if (ctx->miniDumpDirectory.empty())
    {
        // Not set via override so just use Program Data
        PWSTR programDataPath = NULL;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &programDataPath)))
        {
            // Root directory for our executable
            path = programDataPath + std::wstring(L"/NVIDIA/NVIGI/") + file::getExecutableName();
            // Allow up to kMaxAllowedDumps dumps per executable by deleting the oldest one (function is a NOP otherwise)
            fs::path processRoot(path);
            deleteOldestDirectoryIfExceeds(processRoot, kMaxAllowedDumps);
            path += L"/" + extra::utf8ToUtf16(std::to_string(id).c_str());

            if (!file::createDirectoryRecursively(path.c_str()))
            {
                NVIGI_LOG_ERROR("Failed to create folder %S", path.c_str());
                return EXCEPTION_CONTINUE_SEARCH;
            }
        }
        else
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }
    else
    {
        //! Overridden via JSON
        //! 
        //! For simplicity, assuming that the path always exists and there is no need for unique ID so writing nvigi-sha-XXX.dmp directly to this folder
        path = ctx->miniDumpDirectory.wstring().c_str();
    }

    // Log to the log file
    auto filePath = path + L"/nvigi-sha-" + extra::utf8ToUtf16(GIT_LAST_COMMIT_SHORT) + L".dmp";
    NVIGI_LOG_ERROR("Exception detected - thread %u - creating mini-dump '%S'", GetCurrentThreadId(), filePath.c_str());

     // Try to create file for mini dump.
    HANDLE FileHandle = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // Write a mini dump.
    if (FileHandle)
    {
        MINIDUMP_EXCEPTION_INFORMATION DumpExceptionInfo;

        DumpExceptionInfo.ThreadId = GetCurrentThreadId();
        DumpExceptionInfo.ExceptionPointers = exceptionInfo;
        DumpExceptionInfo.ClientPointers = FALSE;

        // Note: callback setup and dump flags taken from https://www.debuginfo.com/examples/src/effminidumps/MidiDump.cpp
        MINIDUMP_CALLBACK_INFORMATION mci{};
        mci.CallbackRoutine = (MINIDUMP_CALLBACK_ROUTINE)nvigiMiniDumpCallback;
        mci.CallbackParam = 0;
        
        MINIDUMP_TYPE dumpFlags = MINIDUMP_TYPE(
            MiniDumpWithPrivateReadWriteMemory |
            MiniDumpWithDataSegs |
            MiniDumpWithHandleData |
            MiniDumpWithFullMemoryInfo |
            MiniDumpWithThreadInfo |
            MiniDumpWithUnloadedModules);
        if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), FileHandle, dumpFlags, &DumpExceptionInfo, NULL, &mci))
        {
            NVIGI_LOG_ERROR( "Failed to create dump - %s", std::system_category().message(GetLastError()).c_str());
        }

        CloseHandle(FileHandle);
    }
    else
    {
        NVIGI_LOG_ERROR( "Failed to create file '%S'", filePath.c_str());
    }
    
    
    //! IMPORTANT: This needs to be done after mini dump is written otherwise stack info in the dump will be messed up
    //! 
    static bool s_symbolsInitialized = false;
    if (!s_symbolsInitialized)
    {
        s_symbolsInitialized = true;
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
    }

    std::string stackTrace;
    if (!dumpStackTrace(exceptionInfo, stackTrace))
    {
        // Not our exception, ignore
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    NVIGI_LOG_INFO("Stack trace:\n%s", stackTrace.c_str());

    // Copy log file next to the mini-dump
    try 
    { 
        auto src = file::normalizePath(std::string(log::getInterface()->getLogPath()) + "/" + log::getInterface()->getLogName());
        fs::path dst = std::wstring(path + L"/" + extra::utf8ToUtf16(log::getInterface()->getLogName()));
        fs::copy(src, dst); 
    }
    catch (std::exception&){}; // safety, don't want any exceptions in the exception handler

    // Close and flush the log
    log::getInterface()->shutdown();

    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI vectoredExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
    return nvigi::exception::writeMiniDump(ExceptionInfo);
}

bool addExceptionHandler(void)
{
    std::scoped_lock lock(ctx->mtx);
    auto exceptionHandler = AddVectoredExceptionHandler(CALL_LAST, vectoredExceptionHandler);
    assert(exceptionHandler != nullptr);
    ctx->exceptionHandlers.push_back(exceptionHandler);
    return exceptionHandler != nullptr;
}

bool removeExceptionHandler(void)
{
    std::scoped_lock lock(ctx->mtx);
    if (ctx->exceptionHandlers.empty()) return false;
    auto res = RemoveVectoredExceptionHandler(ctx->exceptionHandlers.back());
    assert(res != 0);
    ctx->exceptionHandlers.pop_back();
    return res != 0;
}

void setMiniDumpLocation(const char* miniDumpDirectoryPath)
{
    ctx->miniDumpDirectory = nvigi::file::normalizePath(miniDumpDirectoryPath);
    NVIGI_LOG_VERBOSE("Redirecting mini dumps to '%S'", ctx->miniDumpDirectory.wstring().c_str());
}

IException* getInterface()
{
    if (!ctx)
    {
        ctx = new ExceptionContext;
        ctx->iexception.addExceptionHandler = addExceptionHandler;
        ctx->iexception.removeExceptionHandler = removeExceptionHandler;
        ctx->iexception.writeMiniDump = writeMiniDump;
        ctx->iexception.setMiniDumpLocation = setMiniDumpLocation;
    }
    return &ctx->iexception;
}

void destroyInterface()
{
    if (ctx)
    {
        assert(ctx->exceptionHandlers.empty());
        delete ctx;
        ctx = nullptr;
    }
}
}
}

#else

namespace nvigi
{
namespace exception
{

//! Exception handler
//! 
struct ExceptionContext
{
    IException iexception{};
};
static ExceptionContext* ctx{};

bool addExceptionHandler(void)
{
    assert(false && "not implemented");
    return false;
}

bool removeExceptionHandler(void)
{
    assert(false && "not implemented");
    return false;
}


IException* getInterface()
{
    if (!ctx)
    {
        ctx = new ExceptionContext;
        ctx->iexception.addExceptionHandler = addExceptionHandler;
        ctx->iexception.removeExceptionHandler = removeExceptionHandler;
    }
    return &ctx->iexception;
}

void destroyInterface()
{
    if (ctx)
    {
        delete ctx;
    }
}
}
}
#endif // NVIGI_WINDOWS