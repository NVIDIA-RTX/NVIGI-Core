// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <cstdio>
#include <iostream>
#include <thread>
#include <functional>
#include <future>
#include <conio.h>
#include <filesystem>
#include <regex>
#include <cstdint>
#include <cstddef>
#include <windows.h>

#include "source/core/nvigi.api/nvigi.h"
#include "source/core/nvigi.framework/framework.h"
#include "source/core/nvigi.plugin/plugin.h"
#include "source/core/nvigi.extra/extra.h"

namespace fs = std::filesystem;

#define GET_NVIGI_CORE_FUN(F) PFun_##F* F = (PFun_##F*)GetProcAddress(lib, #F)

//! From chatGPT
//!
uint32_t crc32(const std::vector<uint8_t>& data) {
    uint32_t crc = 0xFFFFFFFF;
    for (auto byte : data) {
        crc = crc ^ byte;
        for (int j = 7; j >= 0; j--) {
            crc = (crc >> 1) ^ (0xEDB88320 & uint32_t(-int(crc & 1)));
        }
    }
    return ~crc;
}

const uint32_t CRC24_POLY = 0x864CFB;
const uint32_t CRC24_INIT = 0xB704CE;

uint32_t crc24(const uint8_t* data, size_t length) {
    uint32_t crc = CRC24_INIT;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (static_cast<uint32_t>(data[i]) << 16);
        for (int j = 0; j < 8; ++j) {
            crc <<= 1;
            if (crc & 0x1000000) crc ^= CRC24_POLY;
        }
    }

    return crc & 0xFFFFFF; // Mask to 24 bits
}

// Function to copy text to clipboard
bool copyTextToClipboard(const std::wstring& text) {
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(wchar_t));
    if (!hMem) {
        CloseClipboard();
        return false;
    }

    wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
    if (!pMem) {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    wcscpy_s(pMem, text.length() + 1, text.c_str());

    GlobalUnlock(hMem);
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();

    return true;
}

int makeGUID(GUID& guid, std::string& guidStr, std::string& guidC)
{
    // Generate a GUID
    if (UuidCreate(&guid) != RPC_S_OK) {
        std::cerr << "Failed to generate GUID" << std::endl;
        return 1;
    }

    // Convert the GUID to a string
    wchar_t guid_str[40];
    if (StringFromGUID2(guid, guid_str, 40) == 0) {
        std::cerr << "Failed to convert GUID to string" << std::endl;
        return 1;
    }

    guidStr = nvigi::extra::utf16ToUtf8(guid_str);

    // Convert the GUID to a C-style array string
    std::stringstream array_stream;
    array_stream << "{0x" << std::hex << std::setw(8) << std::setfill('0') << guid.Data1
        << ", 0x" << std::hex << std::setw(4) << std::setfill('0') << guid.Data2
        << ", 0x" << std::hex << std::setw(4) << std::setfill('0') << guid.Data3
        << ",{";
    for (int i = 0; i < 8; ++i) {
        array_stream << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(guid.Data4[i]);
        if (i < 7)
            array_stream << ", ";
    }
    array_stream << "}}";

    guidC = array_stream.str();

    return 0;
}

struct InputParams 
{
    std::string validate{};
    std::string crc24{};
    std::string crc32{};
    std::string _interface{};
    std::string plugin{};
};

void print_usage(int /*argc*/, char** argv, const InputParams& params) {
    fprintf(stderr, "usage: %s [options]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h, --help            show this help message and exit\n");
    fprintf(stderr, "  --interface  NAME     produce GUID for an interface\n");    
    fprintf(stderr, "  --plugin     NAME     produce GUID and crc24 for a plugin\n");
    fprintf(stderr, "  --crc24      NAME     produce crc24 for a given string\n");
    fprintf(stderr, "  --crc32      NAME     produce crc32 for a given string\n");
    fprintf(stderr, "  --validate   DIR      validate all plugins in a directory\n");
    fprintf(stderr, "\n");
}

bool params_parse(int argc, char** argv, InputParams& params) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];        
        if (arg == "-h" || arg == "--help") {
            print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "--interface") {
            if (++i >= argc) {
                printf("Invalid parameter count");
                break;
            }
            params._interface = argv[i];
        }
        else if (arg == "--plugin") {
            if (++i >= argc) {
                printf("Invalid parameter count");
                break;
            }
            params.plugin = argv[i];
        }
        else if (arg == "--crc24") {
            if (++i >= argc) {
                printf("Invalid parameter count");
                break;
            }
            params.crc24 = argv[i];
        }
        else if (arg == "--crc32") {
            if (++i >= argc) {
                printf("Invalid parameter count");
                break;
            }
            params.crc32 = argv[i];
        }
        else if (arg == "--validate") {
            if (++i >= argc) {
                printf("Invalid parameter count");
                break;
            }
            params.validate = argv[i];
        }
        else {
            printf("error: unknown argument: %s\n", arg.c_str());
            print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}

int main(int argc, char** argv)
{
    InputParams params{};
    if (params_parse(argc, argv, params) == false) {
        return 1;
    }

    printf("\n");

    if (!params.plugin.empty())
    {
        std::istringstream iss(params.plugin);
        std::string token;
        std::vector<std::string> items;
        // Extract items using std::getline and '.' as the delimiter
        while (std::getline(iss, token, '.')) 
        {
            if(!token.empty()) items.push_back(token);
        }
        if (items.size() < 3 || items[0] != "nvigi" || items[1] != "plugin")
        {
            printf("Expecting plugin in format 'nvigi.plugin.$name{.$backend.$api}'");
            return 1;
        }
        auto name = items[2];
        auto backend = items.size() > 3 ? items[3] : "";
        auto api = items.size() > 4 ? items[4] : "";
        
        GUID guid;
        std::string guidStr, guidC;
        makeGUID(guid, guidStr, guidC);
        std::string str = "namespace nvigi::plugin::" + name;
        if (!backend.empty())
        {
            str += "::" + backend;
        }
        if (!api.empty())
        {
            str += "::" + api;
        }
        str += "\n{\n";
        str += nvigi::extra::format("constexpr PluginID kId = {{}, 0x{}%x}; //{} [{}]\n", guidC.c_str(), crc24((const uint8_t*)guidStr.c_str(), guidStr.length()), guidStr.c_str(), params.plugin.c_str());
        str += "}\n";
        copyTextToClipboard(nvigi::extra::utf8ToUtf16(str.c_str()));
        printf("%s\n", str.c_str());
        printf("\n\n Results copied to the clipboard");
    }
    if (!params._interface.empty())
    {
        GUID guid;
        std::string guidStr, guidC;
        makeGUID(guid, guidStr, guidC);
        auto str = nvigi::extra::format("\n//! Interface '{}'\n//!\n//! {}\nstruct alignas(8) {}\n{\n    {}() { };\n    NVIGI_UID(UID({}), kStructVersion1)\n\n"
            "    //! v1 members go here, please do NOT break the C ABI compatibility:\n\n"
            "    //! * do not use virtual functions, volatile, STL (e.g. std::vector) or any other C++ high level functionality\n"
            "    //! * do not use nested structures, always use pointer members\n"
            "    //! * do not use internal types in _public_ interfaces (like for example 'nvigi::types::vector' etc.)\n"
            "    //! * do not change or move any existing members once interface has shipped\n\n"
            "    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!\n};\n\nNVIGI_VALIDATE_STRUCT({})\n", params._interface, guidStr, params._interface, params._interface, guidC, params._interface);
        printf("%s\n", str.c_str());
        copyTextToClipboard(nvigi::extra::utf8ToUtf16(str.c_str()));
        printf("\n\n Results copied to the clipboard");
    }
    if (!params.crc24.empty())
    {
        printf("crc24 0x%x : [%s]\n", crc24((const uint8_t*)params.crc24.c_str(), params.crc24.length()), params.crc24.c_str());
    }

    if (!params.crc32.empty())
    {
        printf("crc32 0x%x : [%s]\n", crc32({ (const uint8_t*)params.crc24.c_str(), (const uint8_t*)params.crc24.c_str() + params.crc24.length() }), params.crc24.c_str());
    }

    if(!params.validate.empty())
    {
        params.validate = fs::absolute(params.validate).string();
        if (!fs::is_directory(params.validate))
        {
            printf("%s is not a valid directory", params.validate.c_str());
            return 1;
        }

        printf("Validating SDK located at '%s' ...\n", params.validate.c_str());

        auto libPath = params.validate + "/nvigi.core.framework.dll";
        HMODULE lib = LoadLibraryA(libPath.c_str());
        
        GET_NVIGI_CORE_FUN(nvigiInit);
        GET_NVIGI_CORE_FUN(nvigiShutdown);
        GET_NVIGI_CORE_FUN(nvigiLoadInterface);
        GET_NVIGI_CORE_FUN(nvigiUnloadInterface);

        const char* paths[] =
        {
            params.validate.c_str()
        };

        static bool ok = true;

        auto loggingCallback = [](nvigi::LogType type, const char* msg)->void
        {
            if (type != nvigi::LogType::eInfo)
            {
                ok = false;
                //! Print this so we can see the issue on CI
                printf("%s", msg);
            }
        };

        nvigi::Preferences pref{};
        pref.logLevel = nvigi::LogLevel::eVerbose;
        pref.showConsole = false;
        pref.numPathsToPlugins = _countof(paths);
        pref.utf8PathsToPlugins = paths;
        pref.logMessageCallback = loggingCallback;
        pref.utf8PathToDependencies = params.validate.c_str();
        if (NVIGI_FAILED(error, nvigiInit(pref, nullptr, nvigi::kSDKVersion)))
        {
            printf("error: nvigiInit failed\n");
            return 1;
        }
        if (ok)
        {
            printf("Check: OK\n");
        }
        else
        {
            printf("Check: FAILED\n");
        }
    }
    return 0;
}