// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#ifdef NVIGI_WINDOWS
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <conio.h>
#else
#include <linux/limits.h>
#endif

#include <cstdio>
#include <iostream>
#include <thread>
#include <functional>
#include <future>
#include <filesystem>
#include <regex>
#include <fstream>

namespace fs = std::filesystem;

#include "nvigi.h"
#include "source/core/nvigi.extra/extra.h"
#include "source/core/nvigi.types/types.h"
#include "source/core/nvigi.memory/memory.h"
#include "source/core/nvigi.framework/framework.h"
#include "source/core/nvigi.system/system.h"
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.exception/exception.h"

// Avoid link error with Logging - we don't want it, but thread uses it
#include "source/core/nvigi.log/log.h"
#undef NVIGI_LOG_WARN
#define NVIGI_LOG_WARN(...)
#undef NVIGI_LOG_HINT
#define NVIGI_LOG_HINT(...)
#include "source/core/nvigi.thread/thread.h"

#define CATCH_CONFIG_RUNNER
#include "external/catch2/single_include/catch2/catch.hpp"

#if NVIGI_LINUX
#else
#include "source/tests/ai/d3d12.h"
#include "source/utils/nvigi.wav/wav.h"
#include "source/utils/nvigi.dsound/recorder.h"
#endif

#include "source/plugins/nvigi.hwi/cuda/nvigi_hwi_cuda.h"

#include <cuda_runtime.h>
#include <cuda.h>

namespace nvigi {

namespace memory
{
IMemoryManager* imemory;
IMemoryManager* getInterface() { return imemory;}
}

namespace log
{
ILog* ilog;
ILog* getInterface() { return ilog; }
}

namespace exception
{
IException* iexception;
}

std::chrono::time_point<std::chrono::steady_clock> tstart{};

#define DECLARE_NVIGI_CORE_FUN(F) PFun_##F* F

struct test_params {
    int32_t seed = -1; // RNG seed
    int32_t n_threads = std::min(4, (int32_t)std::thread::hardware_concurrency());
    int32_t n_predict = 100; // new tokens to predict
    int32_t n_context = 4096; // context size
    size_t vram = 8 * 1024;

    // sampling parameters
    int32_t top_k = 40;
    float   top_p = 0.9f;
    float   temp = 0.9f;

    int32_t n_batch = 512; // batch size for prompt processing

    std::string modelDir; // model path
    int32_t iter = 1;

    int logLevel = 2;
    std::string sdkPath;
    std::string depPath;

    DECLARE_NVIGI_CORE_FUN(nvigiInit);
    DECLARE_NVIGI_CORE_FUN(nvigiShutdown);
    DECLARE_NVIGI_CORE_FUN(nvigiLoadInterface);
    DECLARE_NVIGI_CORE_FUN(nvigiUnloadInterface);

    nvigi::system::ISystem* isystem{};
    nvigi::memory::IMemoryManager* imem{};
    nvigi::IHWICuda* icig{};

#ifdef NVIGI_WINDOWS
    bool useCiG = true;
#else
    bool useCiG = false;
#endif
};

test_params params{};

inline std::string getExecutablePath()
{
#ifdef NVIGI_LINUX
    char exePath[PATH_MAX] = {};
    ssize_t size = readlink("/proc/self/exe", exePath, sizeof(exePath));
    (void)size;
    std::string searchPathW = exePath;
    searchPathW.erase(searchPathW.rfind('/'));
    return searchPathW + "/";
#else
    CHAR pathAbsW[MAX_PATH] = {};
    GetModuleFileNameA(GetModuleHandleA(NULL), pathAbsW, ARRAYSIZE(pathAbsW));
    std::string searchPathW = pathAbsW;
    searchPathW.erase(searchPathW.rfind('\\'));
    return searchPathW + "\\";
#endif
}

void loggingCallback(nvigi::LogType /*type*/ , const char* msg)
{
#ifdef NVIGI_WINDOWS
    OutputDebugStringA(msg);
#endif
    std::cout << msg;
}

bool moveFiles(const std::wstring& source_dir, const std::vector<std::wstring>& file_patterns, const fs::path& dst_dir)
{
    // Iterate over each pattern in the list
    for (const auto& pattern : file_patterns) {
        // Convert wildcard pattern to regex
        std::wstring regex_pattern = std::regex_replace(pattern, std::wregex(LR"(\*)"), L".*");
        std::wregex file_regex(regex_pattern);

        for (const auto& entry : fs::directory_iterator(source_dir)) try
        {
            if (fs::is_regular_file(entry.path())) {
                // Match file name against the pattern
                if (std::regex_match(entry.path().filename().wstring(), file_regex)) {
                    // Move matched file to tmp directory
                    fs::path destination = dst_dir / entry.path().filename();
                    fs::rename(entry.path(), destination);
                }
            }
        }
        catch (std::exception& e)
        {
            NVIGI_LOG_ERROR("exception: %s", e.what());
            return false;
        }
    }

    return true;
}

std::wstring moveFilesToTmpLocation(const std::wstring& source_dir, const std::vector<std::wstring>& file_patterns, const std::wstring& dst_dir)
{
    // Create a temporary directory
    fs::path tmp_dir = fs::temp_directory_path() / dst_dir;
    if (fs::exists(tmp_dir))
    {
        fs::remove_all(tmp_dir);
    }
    fs::create_directories(tmp_dir);

    // Iterate over each pattern in the list
    for (const auto& pattern : file_patterns) {
        // Convert wildcard pattern to regex
        std::wstring regex_pattern = std::regex_replace(pattern, std::wregex(LR"(\*)"), L".*");
        std::wregex file_regex(regex_pattern);

        for (const auto& entry : fs::directory_iterator(source_dir)) try
        {
            if (fs::is_regular_file(entry.path())) {
                // Match file name against the pattern
                if (std::regex_match(entry.path().filename().wstring(), file_regex)) {
                    // Move matched file to tmp directory
                    fs::path destination = tmp_dir / entry.path().filename();
                    fs::rename(entry.path(), destination);
                }
            }
        }
        catch (std::exception& e)
        {
            NVIGI_LOG_ERROR("exception: %s", e.what());
        }
    }

    return tmp_dir.wstring();
}

};

#define GET_NVIGI_CORE_FUN(F) nvigi::params.F = (PFun_##F*)GetProcAddress(lib, #F)

#ifdef NVIGI_WINDOWS
TEST_CASE("init_split", "[core]")
{
    auto exePath = nvigi::getExecutablePath();
    auto corePathUtf8 = nvigi::params.sdkPath.empty() ? exePath : nvigi::params.sdkPath;
    auto corePathUtf16 = nvigi::extra::utf8ToUtf16(corePathUtf8.c_str());

    // Split SDK into core and plugins, use unicode 
    std::vector<std::wstring> filesCore = 
    { 
        L"nvigi.core.framework.dll", 
        L"cudart64_12.dll",
        L"nvigi.plugin.hwi.common.dll",
        L"cig_scheduler_settings.dll"
    };
    
    auto corePath = nvigi::moveFilesToTmpLocation(corePathUtf16, filesCore, L"nvigi\\core");

    std::vector<std::wstring> filesSDK =
    {
        L"nvigi.plugin.template*.dll",
    };
    // Test unicode path, nvigi/sdk using cyrillic 
    auto sdkPath = nvigi::moveFilesToTmpLocation(corePathUtf16, filesSDK, L"НВИГИ\\СДК");

    std::vector<std::wstring> filesHWI =
    {
        L"nvigi.plugin.hwi.cuda.dll"
    };
    auto hwiPath = nvigi::moveFilesToTmpLocation(corePathUtf16, filesHWI, L"nvigi\\hwi");

    auto libPath = corePath + L"/nvigi.core.framework.dll";
    HMODULE lib = LoadLibraryW(libPath.c_str());
    REQUIRE(lib != nullptr);

    GET_NVIGI_CORE_FUN(nvigiInit);
    GET_NVIGI_CORE_FUN(nvigiShutdown);
    GET_NVIGI_CORE_FUN(nvigiLoadInterface);
    GET_NVIGI_CORE_FUN(nvigiUnloadInterface);

    REQUIRE(nvigi::params.nvigiInit != nullptr);
    REQUIRE(nvigi::params.nvigiShutdown != nullptr);
    REQUIRE(nvigi::params.nvigiLoadInterface != nullptr);
    REQUIRE(nvigi::params.nvigiUnloadInterface != nullptr);

    corePathUtf8 = nvigi::extra::utf16ToUtf8(corePath.c_str());
    auto sdkPathUtf8 = nvigi::extra::utf16ToUtf8(sdkPath.c_str());
    std::vector<const char*> paths;
    // Note that we are NOT using hwiPath here, we will use it explicitly later when we fetch that interface
    paths.push_back(sdkPathUtf8.c_str());

    // Make sure we restore back files if we crash
    nvigi::extra::ScopedTasks onExit([lib,corePath,sdkPath,hwiPath,filesCore,filesSDK,filesHWI,corePathUtf16]()->void
        {
            // Restore files for the regular init test
            nvigi::moveFiles(corePath, filesCore, corePathUtf16);
            nvigi::moveFiles(sdkPath, filesSDK, corePathUtf16);
            nvigi::moveFiles(hwiPath, filesHWI, corePathUtf16);

            // Cleanup
            FreeLibrary(lib);
            fs::remove_all(sdkPath);
            fs::remove_all(corePath);
            fs::remove_all(hwiPath);
        });

    nvigi::Preferences pref{};
    pref.logLevel = nvigi::params.logLevel ? nvigi::LogLevel::eVerbose : nvigi::LogLevel::eOff;
    pref.showConsole = nvigi::params.logLevel;
    pref.numPathsToPlugins = (uint32_t)paths.size();
    pref.utf8PathsToPlugins = paths.data();
    pref.utf8PathToDependencies = corePathUtf8.c_str();
    //pref.logMessageCallback = nvigi::loggingCallback;
    pref.utf8PathToLogsAndData = corePathUtf8.c_str();
    // Optional info about system and plugins
    nvigi::PluginAndSystemInformation* info{};
    auto result = nvigi::params.nvigiInit(pref, &info, nvigi::kSDKVersion);
    REQUIRE(result == nvigi::kResultOk);
    REQUIRE(info->numDetectedAdapters >= 1);
    REQUIRE(info->numDetectedPlugins >= 1);

    // Make sure additional path to plugins works as expected
    auto prevNumPlugins = info->numDetectedPlugins;
    nvigi::IHWICuda* icig0{};
    // Load the plugin from the additional path
    nvigiGetInterfaceDynamic(nvigi::plugin::hwi::cuda::kId, &icig0, nvigi::params.nvigiLoadInterface, nvigi::extra::utf16ToUtf8(hwiPath.c_str()).c_str());
    REQUIRE(icig0 != nullptr);
    nvigi::IHWICuda* icig1{};
    // Load the same plugin, make sure it is not duplicated
    nvigiGetInterfaceDynamic(nvigi::plugin::hwi::cuda::kId, &icig1, nvigi::params.nvigiLoadInterface, nvigi::extra::utf16ToUtf8(hwiPath.c_str()).c_str());
    REQUIRE(icig1 != nullptr);
    // hwi.cuda depends on hwi.common, so we should have 2 plugins loaded
    auto newNumPlugins = info->numDetectedPlugins;
    REQUIRE(newNumPlugins == (prevNumPlugins + 2));
    result = nvigi::params.nvigiUnloadInterface(nvigi::plugin::hwi::cuda::kId, icig0);
    REQUIRE(result == nvigi::kResultOk);
    result = nvigi::params.nvigiUnloadInterface(nvigi::plugin::hwi::cuda::kId, icig1);
    REQUIRE(result == nvigi::kResultOk);

    result = nvigi::params.nvigiShutdown();
    REQUIRE(result == nvigi::kResultOk);
}
#endif


//! INIT FRAMEWORK
//! 
TEST_CASE("init", "[core]")
{
    auto exePath = nvigi::getExecutablePath();
    auto corePathUtf8 = nvigi::params.sdkPath.empty() ? exePath : nvigi::params.sdkPath;

#ifdef NVIGI_WINDOWS
    auto libPath = corePathUtf8 + "/nvigi.core.framework.dll";
#else
    auto libPath = corePathUtf8 + "/nvigi.core.framework.so";
#endif
    auto finalPath = nvigi::file::normalizePath(nvigi::extra::utf8ToUtf16(libPath.c_str()));
    HMODULE lib = LoadLibraryW(finalPath.wstring().c_str());
    REQUIRE(lib != nullptr);

    GET_NVIGI_CORE_FUN(nvigiInit);
    GET_NVIGI_CORE_FUN(nvigiShutdown);
    GET_NVIGI_CORE_FUN(nvigiLoadInterface);
    GET_NVIGI_CORE_FUN(nvigiUnloadInterface);

    REQUIRE(nvigi::params.nvigiInit != nullptr);
    REQUIRE(nvigi::params.nvigiShutdown != nullptr);
    REQUIRE(nvigi::params.nvigiLoadInterface != nullptr);
    REQUIRE(nvigi::params.nvigiUnloadInterface != nullptr);

    std::vector<const char*> paths;
    if (!nvigi::params.sdkPath.empty())
    {
        paths.push_back(nvigi::params.sdkPath.c_str());
    }
    paths.push_back(exePath.c_str());
        
    nvigi::Preferences pref{};
    pref.logLevel = nvigi::params.logLevel ? nvigi::LogLevel::eVerbose : nvigi::LogLevel::eOff;
    pref.showConsole = nvigi::params.logLevel;
    pref.numPathsToPlugins = (uint32_t)paths.size();
    pref.utf8PathsToPlugins = paths.data();
    pref.utf8PathToDependencies = nvigi::params.depPath.empty() ? corePathUtf8.c_str() : nvigi::params.depPath.c_str();
    //pref.logMessageCallback = nvigi::loggingCallback;
    pref.utf8PathToLogsAndData = corePathUtf8.c_str();
    // Optional info about system and plugins
    nvigi::PluginAndSystemInformation* info{};
    auto result = nvigi::params.nvigiInit(pref,&info,nvigi::kSDKVersion);
    REQUIRE(result == nvigi::kResultOk);
    REQUIRE(info->numDetectedAdapters >= 1);
    REQUIRE(info->numDetectedPlugins >= 1);


#ifdef NVIGI_WINDOWS
    //! Test "getters" but only on Windows since there is no hwi::cuda on Linux
    nvigi::Version version;
    nvigi::VendorId vendor;
    uint32_t arch;
    REQUIRE(nvigi::getPluginStatus(info, nvigi::plugin::hwi::cuda::kId) == nvigi::kResultOk);
    REQUIRE(nvigi::getPluginRequiredOSVersion(info, nvigi::plugin::hwi::cuda::kId,version) == nvigi::kResultOk);
    REQUIRE(version == nvigi::Version(10, 0, 19041)); // HW scheduling introduced - Windows 10, Version 2004 (20H1), which corresponds to build 19041 (Windows 10 May 2020 Update).
    REQUIRE(nvigi::getPluginRequiredAdapterDriverVersion(info, nvigi::plugin::hwi::cuda::kId, version) == nvigi::kResultOk);
    REQUIRE(version == nvigi::Version(555, 85, 0)); // CiG min spec
    REQUIRE(nvigi::getPluginRequiredAdapterVendor(info, nvigi::plugin::hwi::cuda::kId, vendor) == nvigi::kResultOk);
    REQUIRE(vendor == nvigi::VendorId::eNVDA);
    REQUIRE(nvigi::getPluginRequiredAdapterArchitecture(info, nvigi::plugin::hwi::cuda::kId, arch) == nvigi::kResultOk);
    REQUIRE(arch == 0x140);
    REQUIRE(nvigi::isPluginExportingInterface(info, nvigi::plugin::hwi::cuda::kId, nvigi::IHWICuda::s_type) == nvigi::kResultOk);
#endif

    //! IMPORTANT: Unit test is using INTERNAL interfaces for convenience and validation
    //!
    //! Host application will not have access to the "nvigi::core::framework::kId" hence no access to the internal interfaces
    nvigiGetInterfaceDynamic(nvigi::core::framework::kId, &nvigi::params.isystem, nvigi::params.nvigiLoadInterface);
    REQUIRE(nvigi::params.isystem != nullptr);
    nvigiGetInterfaceDynamic(nvigi::core::framework::kId, &nvigi::memory::imemory, nvigi::params.nvigiLoadInterface);
    REQUIRE(nvigi::memory::imemory != nullptr);
    nvigi::params.imem = nvigi::memory::imemory;
    nvigiGetInterfaceDynamic(nvigi::core::framework::kId, &nvigi::log::ilog, nvigi::params.nvigiLoadInterface);
    REQUIRE(nvigi::log::ilog != nullptr);

    if (nvigi::params.useCiG)
    {
        cuInit(0);

        auto res = nvigiGetInterfaceDynamic(nvigi::plugin::hwi::cuda::kId, &nvigi::params.icig, nvigi::params.nvigiLoadInterface);
        bool successOrOldDriver = (res == nvigi::kResultOk) || (res == nvigi::kResultDriverOutOfDate);
        REQUIRE(successOrOldDriver);

        if (res == nvigi::kResultDriverOutOfDate)
        {
            NVIGI_LOG_TEST_WARN("Driver cannot support shared CUDA context, rest of test will be skipped");
            nvigi::params.useCiG = false;
        }
    }

    NVIGI_LOG_TEST_INFO("Starting unit testing ...");
}

//! TYPES
//! 
#include "source/core/nvigi.types/tests.h"

//! CUDA/CiG
//! 
#ifdef NVIGI_WINDOWS
#include "source/plugins/nvigi.hwi/cuda/tests.h"
#include "source/plugins/nvigi.template.inference.cuda/backend/tests.h"
#endif

// We test the template tests to make sure that they will compile after the 
// plugin writer copy-pastes them
#include "source/plugins/nvigi.template.generic/backend/tests.h"
#include "source/plugins/nvigi.template.inference/backend/tests.h"

//! TODO: ALL NEW PLUGIN TESTS GO HERE
//! 
#include "source/utils/nvigi.ai/tests.h"

// DO not add tests after this block without consulting the dev team; active experiments with the CUDA-related tests

//! SHUTDOWN FRAMEWORK
//! 
TEST_CASE("shutdown","[core]")
{
    if (nvigi::params.icig)
    {
        nvigi::Result res = nvigi::params.nvigiUnloadInterface(nvigi::plugin::hwi::cuda::kId, nvigi::params.icig);
        REQUIRE(res == nvigi::kResultOk);
    }

#ifdef NVIGI_WINDOWS
    REQUIRE(nvigi::D3D12ContextInfo::GetActiveInstance() == nullptr);
#endif
    nvigi::params.nvigiUnloadInterface(nvigi::core::framework::kId, nvigi::params.isystem);
    nvigi::params.isystem = nullptr;
    nvigi::params.nvigiUnloadInterface(nvigi::core::framework::kId, nvigi::memory::imemory);
    nvigi::memory::imemory = nullptr;
    nvigi::params.nvigiUnloadInterface(nvigi::core::framework::kId, nvigi::log::ilog);
    nvigi::log::ilog = nullptr;
    auto result = nvigi::params.nvigiShutdown();
    REQUIRE(result == nvigi::kResultOk);
}

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"

// Custom event listener class
class TestRunListener : public Catch::TestEventListenerBase {
public:
    using TestEventListenerBase::TestEventListenerBase; // Inherit constructor

    // Called before a test case starts
    void testCaseStarting(Catch::TestCaseInfo const& /*testInfo*/) override
    {
        if (nvigi::params.isystem)
        {
            nvigi::system::VRAMUsage* usage;
            nvigi::params.isystem->getVRAMStats(0, &usage);
            this->startVRAMUsageMB = usage->currentUsageMB;
        }
    }

    // Called after a test case ends
    void testCaseEnded(Catch::TestCaseStats const& testCaseStats) override 
    {
        if (nvigi::params.isystem)
        {
            nvigi::system::VRAMUsage* usage;
            nvigi::params.isystem->getVRAMStats(0, &usage);
            double vramDelta = (double)usage->currentUsageMB - (double)this->startVRAMUsageMB;
            if (vramDelta > 0.1)
            {
                NVIGI_LOG_TEST_ERROR("### VRAM LEAK ### - test %s leaked %fMB", testCaseStats.testInfo.name.c_str(), vramDelta);
            }
        } 
    }

    size_t startVRAMUsageMB;
};

// Register the custom listener
CATCH_REGISTER_LISTENER(TestRunListener);

//#define NVIGI_TEST_DEBUG_GRPC

#ifdef NVIGI_LINUX
int main(int argc, char* argv[])
#else
//! Allow utf-8 paths on the command line
int wmain(int argc, wchar_t** argv)
#endif
{
    FILE * f{}; 
#ifdef NVIGI_WINDOWS    
    freopen_s(&f, "NUL", "w", stderr);
#else
    f = freopen("/dev/nul", "w", stderr);
    (void)f;
#endif

#ifdef NVIGI_WINDOWS
    //! Testing SDKs ability to load plugins correctly and restore host settings
    SetDllDirectoryA("c:\\");
#endif

    // Convert wide arguments to UTF-8
    std::vector<std::string> utf8_args;
    utf8_args.reserve(argc);
#ifdef NVIGI_LINUX
    // Vector to store the wide character arguments
    std::vector<wchar_t*> wargv(argc);

    // Convert each argument from multibyte to wide character
    for (int i = 0; i < argc; ++i) {
        // Allocate memory for the wide character argument
        size_t len = mbstowcs(nullptr, argv[i], 0) + 1;
        wargv[i] = new wchar_t[len];

        // Perform the conversion
        mbstowcs(wargv[i], argv[i], len);
    }
    for (int i = 0; i < argc; ++i) {
        utf8_args.push_back(nvigi::extra::utf16ToUtf8(wargv[i]));
    }
     // Clean up allocated memory
    for (int i = 0; i < argc; ++i) {
        delete[] wargv[i];
    }
#else
    for (int i = 0; i < argc; ++i) {
        utf8_args.push_back(nvigi::extra::utf16ToUtf8(argv[i]));
    }
#endif
    // Convert UTF-8 arguments to char* array for Catch2
    std::vector<const char*> catch2_argv;
    catch2_argv.reserve(argc);
    for (const auto& arg : utf8_args) {
        catch2_argv.push_back(arg.c_str());
    }

    Catch::Session session; // Creating a new session

    // Build a new parser on top of Catch's
    using namespace Catch::clara;
    auto cli = session.cli() 
        | Opt(nvigi::params.sdkPath, "sdk path")
        ["--sdk"]
        ("path to the nvigi sdk")
        
        | Opt(nvigi::params.depPath, "dependencies path")
        ["--dependencies"]
        ("path to the nvigi dependencies");

    // Now pass the new composite back to Catch so it uses that
    session.cli(cli);

    // Apply command-line arguments to the session
    int returnCode = session.applyCommandLine(argc, catch2_argv.data());
    if (returnCode != 0) {
        // Command line parsing error
        return returnCode;
    }

    // Run the tests
    int numFailed = session.run(); 

    return numFailed;
}