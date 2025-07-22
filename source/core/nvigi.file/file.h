// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <sys/stat.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <inttypes.h>
#include <fstream>
#include <filesystem>
#include <random>

#include "source/core/nvigi.extra/extra.h"

#ifdef NVIGI_WINDOWS
#include <KnownFolders.h>
#include <ShlObj.h>
EXTERN_C IMAGE_DOS_HEADER __ImageBase; // MS linker feature
#else
#include <linux/limits.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace nvigi
{
namespace file
{

// Account for long paths on Windows so no legacy MAX_PATH
constexpr size_t kMaxFilePath = 4096;

inline bool exists(const wchar_t* src)
{
    return fs::exists(src);
}

inline bool copy(const wchar_t* dst, const wchar_t* src)
{
    return fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
}

inline void write(const wchar_t* fname, const std::vector<uint8_t>& data)
{
    fs::path p(fname);
#ifdef NVIGI_WINDOWS
    std::fstream file(fname, std::ios::binary | std::ios::out);
#else
    std::fstream file(extra::utf16ToUtf8(fname).c_str(), std::ios::binary | std::ios::out);
#endif
    file.write((const char*)data.data(), data.size());
}

inline FILE* open(const wchar_t* path, const wchar_t* mode)
{
#if NVIGI_WINDOWS    
    FILE* file = _wfsopen(path, mode, _SH_DENYNO);
#else
    FILE* file = fopen(extra::utf16ToUtf8(path).c_str(), extra::utf16ToUtf8(mode).c_str());
#endif
    if (!file)
    {
        if (errno != ENOENT)
        {
            NVIGI_LOG_ERROR( "Unable to open file %S - error = %d", path, errno);
        }
        else
        {
            NVIGI_LOG_ERROR( "File '%S' does not exist", path);
        }
    }
    return file;
}

inline void flush(FILE* file)
{
    fflush(file);
}

inline void close(FILE* file)
{
    fclose(file);
}

//! Attempt to read data of specified size from file.
//! Returns number of bytes read.
//! 
//! IMPORTANT: strings (char*/wchar_t*) won't be null-terminated unless the file contents
//! contain it and chunkSize includes it.  Make sure to null-terminate strings where
//! required.
inline size_t readChunk(FILE* file, void* chunk, size_t chunkSize)
{
    return fread(chunk, 1, chunkSize, file);
}

inline size_t writeChunk(FILE* file, const void* chunk, const size_t chunkSize)
{
    return fwrite(chunk, 1, chunkSize, file);
}

inline char* readLine(FILE* file, char* line, size_t maxLineSize)
{
    char* stringRead = fgets(line, int(maxLineSize), file);
    if (stringRead)
    {
        // Remove line endings (Linux and Windows)
        auto LF = '\n';
        auto CR = '\r';
        size_t end = strlen(stringRead) - 1;
        if (stringRead[end] == LF)
        {
            stringRead[end] = '\0';
            if (end > 0 && stringRead[end - 1] == CR)
            {
                stringRead[end - 1] = '\0';
            }
        }
    }
    return stringRead;
}

inline bool writeLine(FILE* file, const char* line)
{
    size_t lineLen = strlen(line);
    size_t written = fwrite(line, 1, lineLen, file);
    if (written != lineLen)
    {
        return false;
    }
    auto LF = '\n';
    int ret = fputc(LF, file);
    return ret == LF;
}

inline std::string readAsStr(const char* filename)
{
    std::ifstream file(filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

inline std::vector<uint8_t> read(const char* fname)
{
    fs::path p(fname);
    size_t file_size = fs::file_size(p);
    std::vector<uint8_t> ret_buffer(file_size);
#ifdef NVIGI_LINUX
    std::fstream file(fname, std::ios::binary | std::ios::in);
#else
    std::fstream file(fname, std::ios::binary | std::ios::in);
#endif
    file.read((char*)ret_buffer.data(), file_size);
    return ret_buffer;
}

inline std::vector<uint8_t> read(const wchar_t* fname)
{
    fs::path p(fname);
    size_t file_size = fs::file_size(p);
    std::vector<uint8_t> ret_buffer(file_size);
#ifdef NVIGI_LINUX
    std::fstream file(extra::toStr(fname), std::ios::binary | std::ios::in);
#else
    std::fstream file(fname, std::ios::binary | std::ios::in);
#endif
    file.read((char*)ret_buffer.data(), file_size);
    return ret_buffer;
}

inline const wchar_t* getTmpPath()
{
    static std::wstring g_result;
    g_result = fs::temp_directory_path().wstring();
    return g_result.c_str();
}

// Required when using symlinks
inline std::string getRealPath(const char* filename)
{
#ifdef NVIGI_WINDOWS
    auto file = CreateFileA(filename,   // file to open
        GENERIC_READ,          // open for reading
        FILE_SHARE_READ,       // share for reading
        NULL,                  // default security
        OPEN_EXISTING,         // existing file only
        FILE_ATTRIBUTE_NORMAL, // normal file
        NULL);                 // no attr. template  
    char buffer[kMaxFilePath] = {};
    GetFinalPathNameByHandleA(file, buffer, kMaxFilePath, FILE_NAME_OPENED);
    CloseHandle(file);
#else
    char buffer[PATH_MAX] = {};
    ::realpath(filename, buffer);
#endif
    return std::string(buffer);
}


inline time_t getModTime(const std::string& pathAbs)
{
    std::string path = getRealPath(pathAbs.c_str());
    auto t = fs::last_write_time(path);
    return t.time_since_epoch().count();
}

inline const wchar_t* getCurrentDirectoryPath()
{
    static std::wstring s_path;
    s_path = fs::current_path().wstring();
    return s_path.c_str();
}

inline bool setCurrentDirectoryPath(const wchar_t* path)
{
    std::error_code ec;
    fs::current_path(path, ec);
    return !ec;
}

inline std::string removeExtension(const std::string& filename)
{
    size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

inline bool remove(const wchar_t* path)
{
    bool success = false;
#if NVIGI_WINDOWS    
    SHFILEOPSTRUCTW fileOperation;
    fileOperation.wFunc = FO_DELETE;
    fileOperation.pFrom = path;
    fileOperation.fFlags = FOF_NO_UI | FOF_NOCONFIRMATION;

    int result = ::SHFileOperationW(&fileOperation);
    if (result != 0)
    {
        NVIGI_LOG_ERROR( "Failed to delete file '%S' (error code: %d)", path, result);
    }
    else
    {
        success = true;
    }

#else
    success = remove(path) == 0;
#endif

    return success;
}

inline bool move(const wchar_t* from, const wchar_t* to)
{
#if NVIGI_WINDOWS
    if (!MoveFileW(from, to))
    {
        NVIGI_LOG_ERROR( "File move failed: '%S' -> '%S' (error code %" PRIu32 ")", from, to, GetLastError());
        return false;
    }
    return true;
#else
    if (rename(extra::toStr(from).c_str(), extra::toStr(to).c_str()) < 0)
    {
        NVIGI_LOG_ERROR("File move failed: '%S' -> '%S' (%s)", from, to, strerror(errno));
        return false;
    }
    return true;
#endif
}

inline bool createDirectoryRecursively(const wchar_t* path)
{
    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec)
    {
        NVIGI_LOG_ERROR( "createDirectoryRecursively failed with %s", ec.message().c_str());
        return false;
    }
    return true;
}

inline std::wstring getModulePath()
{
    std::wstring res;
#ifdef NVIGI_WINDOWS
    wchar_t modulePath[kMaxFilePath] = { 0 };
    GetModuleFileNameW((HINSTANCE)&__ImageBase, modulePath, kMaxFilePath);
    fs::path dllPath(modulePath);
    dllPath.remove_filename();
    res = dllPath.c_str();
#endif
    return res;
}

inline std::wstring getExecutablePath()
{
#ifdef NVIGI_LINUX
    char exePath[PATH_MAX] = {};
    ssize_t size = readlink("/proc/self/exe", exePath, sizeof(exePath));
    (void)size;
    std::wstring searchPathW = extra::toWStr(exePath);
    searchPathW.erase(searchPathW.rfind('/'));
    return searchPathW + L"/";
#else
    WCHAR pathAbsW[kMaxFilePath] = {};
    GetModuleFileNameW(GetModuleHandleA(NULL), pathAbsW, ARRAYSIZE(pathAbsW));
    std::wstring searchPathW = pathAbsW;
    searchPathW.erase(searchPathW.rfind('\\'));
    return searchPathW + L"\\";
#endif
}

inline std::wstring getExecutableName()
{
#ifdef NVIGI_LINUX
    char exePath[PATH_MAX] = {};
    readlink("/proc/self/exe", exePath, sizeof(exePath));
    return extra::toWStr(exePath);
#else
    WCHAR pathAbsW[kMaxFilePath] = {};
    GetModuleFileNameW(GetModuleHandleA(NULL), pathAbsW, ARRAYSIZE(pathAbsW));
    std::wstring searchPathW = pathAbsW;
    searchPathW = searchPathW.substr(searchPathW.rfind('\\') + 1);
    searchPathW.erase(searchPathW.rfind('.'));
    return searchPathW;
#endif
}

inline std::wstring getExecutableNameAndExtension()
{
#ifdef NVIGI_LINUX
    char exePath[PATH_MAX] = {};
    readlink("/proc/self/exe", exePath, sizeof(exePath));
    return extra::toWStr(exePath);
#else
    WCHAR pathAbsW[kMaxFilePath] = {};
    GetModuleFileNameW(GetModuleHandleA(NULL), pathAbsW, ARRAYSIZE(pathAbsW));
    std::wstring searchPathW = pathAbsW;
    searchPathW = searchPathW.substr(searchPathW.rfind('\\') + 1);
    return searchPathW;
#endif
}

inline bool isRelativePath(const std::wstring& path)
{
    return fs::path(path).is_relative();
}

// Function to normalize a path: makes it absolute and removes trailing slashes
inline fs::path normalizePath(const fs::path& path)
{
    fs::path absPath = fs::absolute(fs::canonical(path));
    // Check if the path ends with a separator and is not a root directory
    if (!absPath.empty() && absPath != absPath.root_path() && absPath.filename() == "") {
        // Remove the trailing separator by returning the parent path
        return absPath.parent_path();
    }
    return absPath;
};

#ifdef NVIGI_WINDOWS

inline bool getCurrectDLLDirectory(std::wstring& path)
{
    wchar_t prevUtf16Directory[kMaxFilePath]{};
    if (GetDllDirectoryW(kMaxFilePath, prevUtf16Directory) > 0)
    {
        path = prevUtf16Directory;
        return true;
    }
    return false;
}

inline bool getSystem32Path(std::wstring& path) 
{
    wchar_t systemDir[kMaxFilePath];
    // Retrieves the path to the System32 directory
    UINT length = GetSystemDirectoryW(systemDir, kMaxFilePath);
    if (length == 0) {
        return false;
    }
    path = systemDir;
    return true; 
}

// Tell Win32 to load DLLs from this directory otherwise 3rd party dependencies won't be found
class ScopedDLLSearchPathChange
{
public:
    ScopedDLLSearchPathChange(std::vector<std::wstring>& utf16DependeciesDirectories)
    {
        for (auto& utf16DependeciesDirectory : utf16DependeciesDirectories)
        {
            if (std::find(utf16DependeciesDirectoriesAlreadyAdded.begin(), utf16DependeciesDirectoriesAlreadyAdded.end(), utf16DependeciesDirectory) != utf16DependeciesDirectoriesAlreadyAdded.end())
            {
                // No point in duplicating search paths so skip as needed
                continue;
            }
            //! Add our path to the search list
            if (DLL_DIRECTORY_COOKIE dllDirCookie;!(dllDirCookie = AddDllDirectory(utf16DependeciesDirectory.c_str())))
            {
                NVIGI_LOG_WARN("AddDllDirectory failed with last error '%u' - if 3rd party DLLs are not next to the executable some plugins might fail to load!", GetLastError());
            }
            else
            {
                dllDirCookies.push_back(dllDirCookie);
                utf16DependeciesDirectoriesAlreadyAdded.push_back(utf16DependeciesDirectory);
                NVIGI_LOG_INFO("Looking for 3rd party dependencies in `%S`", utf16DependeciesDirectory.c_str());
            }
        }
    }
    ~ScopedDLLSearchPathChange()
    {
        for (auto& dllDirCookie : dllDirCookies)
        {
            //! Remove our path from the search list
            if (!RemoveDllDirectory(dllDirCookie))
            {
                NVIGI_LOG_WARN("RemoveDllDirectory failed with last error '%u' - previous DLL directory not restored", GetLastError());
            }
        }
        dllDirCookies.clear();
        utf16DependeciesDirectoriesAlreadyAdded.clear();
    }

private:
    std::vector<std::wstring> utf16DependeciesDirectoriesAlreadyAdded{};
    std::vector<DLL_DIRECTORY_COOKIE> dllDirCookies{};
};
#endif

constexpr const char* kWindowsLongPathPrefix = "\\\\?\\";

//! Converts incoming utf-8 path to an OS valid path (can be file of directory and it must exist)
//! 
//! On Linux there is not much to do but make sure correct slashes are used
//! On Windows we need to take care of the long paths and handle them correctly to avoid legacy issues with MAX_PATH
//! 
//! More details: https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry
//!
inline void prependLongPathPrefix(std::u8string& _path)
{
#ifdef NVIGI_WINDOWS
    // On Windows long paths must be marked correctly otherwise, due to backwards compatibility, all paths are limited to MAX_PATH length
    if (_path.size() >= MAX_PATH)
    {
        // Must be prefixed with "\\?\" to be considered long path
        if (_path.substr(0, 4) != ((const char8_t*)kWindowsLongPathPrefix))
        {
            // Long path but not "tagged" correctly
            _path = ((const char8_t*)kWindowsLongPathPrefix) + _path;
        }
    }
#else
    // Reference unused parameters.
    (void)_path;
#endif
}

inline bool getOSValidPath(const std::u8string& _path, std::u8string& _out)
{
    std::u8string path(_path);
    prependLongPathPrefix(path);
    if (!fs::exists(path))
    {
        return false;
    }
    _out = file::normalizePath(path).u8string();

    // Need to prepend again if the normalized path is longer than MAX_PATH
    prependLongPathPrefix(_out);
    return true;
}

//! Helpers for directories - see getOSValidPath for details
//! 
//! NOTE: We store UTF8 as regular std::string for simplicity since inputs from host are const char*
//! 
inline bool getOSValidDirectoryPath(const char* utf8PathIn, std::string& utf8PathOut)
{
    std::u8string p1((const char8_t*)utf8PathIn);
    std::u8string p2;
    if (!getOSValidPath(p1, p2) || !fs::is_directory(p2))
    {
        return false;
    }
    utf8PathOut = (const char*)p2.c_str();
    return true;
}

}
}
