// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#ifdef NVIGI_WINDOWS
// Prevent warnings from MS headers
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <Winternl.h>
#include <d3dkmthk.h>
#include <d3dkmdt.h>
#include <dxgi1_6.h>
#include "external/nvapi/nvapi.h"
#else
#include <array>
#include <memory>
#include <regex>
#include <sys/utsname.h>
#endif

#include "source/core/nvigi.system/system.h"
#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.api/nvigi_version.h"
#include "source/core/nvigi.api/nvigi_types.h"

#define NVAPI_VALIDATE_RF(f) {auto r = f; if(r != NVAPI_OK) { NVIGI_LOG_ERROR( "%s failed error %d", #f, r); return false;} };

namespace nvigi
{

namespace security
{
// included in framwork.cpp
bool verifyEmbeddedSignature(const wchar_t* pathToFile);
}

namespace system
{

#ifdef NVIGI_LINUX
Result downgradePrivileges() { return kResultOk; };
Result restorePrivileges() { return kResultOk; };
#else // NVIGI_WINDOWS

//! Based on https://learn.microsoft.com/en-us/windows/win32/secauthz/enabling-and-disabling-privileges-in-c--
//! 
//! If process is running with elevated privileges we downgrade them temporarily
struct Privileges
{
    bool hasAdminRights() { return runningAsAdmin; }
    

    HANDLE hToken{};
    PreferenceFlags flags{};
    std::tuple< std::vector<uint8_t>, DWORD> prevPrivileges;
    bool runningAsAdmin = false;
    bool ranOnce = false;
};
static Privileges s_privileges{};

bool isProcessRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroupSid = NULL;

    // Create a SID for the Administrators group.
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroupSid)) {
        return false;
    }

    // Check if the token contains the admin group SID
    if (!CheckTokenMembership(NULL, adminGroupSid, &isAdmin)) {
        isAdmin = FALSE; // Default to false if check fails
    }

    // Clean up SID
    FreeSid(adminGroupSid);
    return isAdmin;
}

std::wstring getPrivilegeNameFromLUID(LUID& luid) {
    DWORD nameLength = 0;

    // First, call LookupPrivilegeName to get the required buffer size.
    LookupPrivilegeNameW(NULL, &luid, NULL, &nameLength);

    // Allocate buffer with the required size
    std::wstring privilegeName(nameLength, L'\0');

    // Now, call LookupPrivilegeName again with the allocated buffer.
    if (LookupPrivilegeName(NULL, &luid, &privilegeName[0], &nameLength)) {
        // Adjust the string size to the actual name length.
        privilegeName.resize(nameLength);
        return privilegeName;
    }
    else {
        // If the function fails, return an empty string or handle error as needed.
        return L"";
    }
}

bool disablePrivileges(HANDLE hToken, std::vector<LPCWSTR>& lpszPrivileges)
{
    // Windows has this weird system with ANYSIZE_ARRAY structures so one needs to allocate memory this way
    std::vector<uint8_t> buffer(sizeof(TOKEN_PRIVILEGES) + (lpszPrivileges.size() - 1) * sizeof(LUID_AND_ATTRIBUTES));
    TOKEN_PRIVILEGES& tp = *(TOKEN_PRIVILEGES*)buffer.data();
    // Previous values
    std::vector<uint8_t> buffer1(sizeof(TOKEN_PRIVILEGES) + (lpszPrivileges.size() - 1) * sizeof(LUID_AND_ATTRIBUTES));
    auto ptp = (TOKEN_PRIVILEGES*)buffer1.data();

    tp.PrivilegeCount = 0;
    // Read original values first
    for (auto lpszPrivilege : lpszPrivileges)
    {
        LUID luid;
        if (!LookupPrivilegeValue(
            NULL,           // Lookup privilege on local system
            lpszPrivilege,  // Privilege to lookup 
            &luid)) {       // Receives LUID of privilege
            NVIGI_LOG_ERROR("LookupPrivilegeValue error: %s", GetLastError());
            return false;
        }
     
        tp.Privileges[tp.PrivilegeCount].Luid = luid;
        tp.Privileges[tp.PrivilegeCount].Attributes = 0;
        tp.PrivilegeCount++;
    }

    DWORD returnSize = (DWORD)buffer1.size();
    // Adjust the token privileges in atomic fashion
    if (!AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        (DWORD)buffer.size(),
        ptp,
        &returnSize)) 
    {
        NVIGI_LOG_ERROR("AdjustTokenPrivileges error: %s", std::system_category().message(GetLastError()).c_str());
        return false;
    }

    // These are the actual privileges that changed, can be less than what we requested to downgrade
    if (!s_privileges.ranOnce)
    {
        auto i = ptp->PrivilegeCount;
        while (i--)
        {
            NVIGI_LOG_WARN("Privilege '%S' downgraded", getPrivilegeNameFromLUID(ptp->Privileges[i].Luid).c_str());
        }
    }

    s_privileges.prevPrivileges = { buffer1, returnSize };
    return true;
}

Result downgradePrivileges()
{
    if ((s_privileges.flags & PreferenceFlags::eDisablePrivilegeDowngrade))
    {
        // Nothing to do, host override is in place
        return kResultOk;
    }

    if (!s_privileges.ranOnce)
    {
        s_privileges.runningAsAdmin = isProcessRunningAsAdmin();
        if (s_privileges.runningAsAdmin)
        {
            NVIGI_LOG_WARN("Running with elevated privileges - attempt will be made to downgrade some of them while running NVIGI SDK modules");
        }
        else
        {
            s_privileges.ranOnce = true;
        }
    }

    if (!s_privileges.runningAsAdmin)
    {
        // Nothing to do, not running as admin
        return kResultOk;
    }

    
    // Open the current process token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &s_privileges.hToken))
    {
        NVIGI_LOG_ERROR("OpenProcessToken error: %s", std::system_category().message(GetLastError()).c_str());
        return kResultInvalidState;
    }

    // Disable key privileges
    //
    // NOTE: Not all necessarily will be granted/enabled so not all will be downgraded
    std::vector<LPCWSTR> privileges =
    { 
        SE_LOAD_DRIVER_NAME,
        SE_DEBUG_NAME,
        SE_TCB_NAME,
        SE_ASSIGNPRIMARYTOKEN_NAME,
        SE_SHUTDOWN_NAME,
        SE_BACKUP_NAME,
        SE_RESTORE_NAME,
        SE_TAKE_OWNERSHIP_NAME,
        SE_IMPERSONATE_NAME 
    };
    bool success = disablePrivileges(s_privileges.hToken,privileges);

    if (!success)
    {
        NVIGI_LOG_ERROR("Failed to disable privilege(s).");
        CloseHandle(s_privileges.hToken);
        s_privileges.hToken = nullptr;
        return kResultInvalidState;
    }

    if (!s_privileges.ranOnce)
    {
        s_privileges.ranOnce = true;
    }
    return kResultOk;
}

Result restorePrivileges()
{
    if ((s_privileges.flags & PreferenceFlags::eDisablePrivilegeDowngrade) || !s_privileges.runningAsAdmin)
    {
        // Nothing to do, not running as admin or host override is in place
        return kResultOk;
    }

    // Clean up and restore privileges
    if (s_privileges.hToken)
    {
        
        auto& [buffer, size] = s_privileges.prevPrivileges;
        auto ptp = *(TOKEN_PRIVILEGES*)buffer.data();

        // Restore token privilege
        if (!AdjustTokenPrivileges(
            s_privileges.hToken,
            FALSE,
            &ptp,
            size,
            NULL,
            (PDWORD)NULL)) {
            NVIGI_LOG_ERROR("AdjustTokenPrivileges error: ", std::system_category().message(GetLastError()).c_str());
            return kResultInvalidState;
        }

        CloseHandle(s_privileges.hToken);
        s_privileges.hToken = nullptr;
    }
    return kResultOk;
}

// Based on https://stackoverflow.com/questions/15960437/how-to-read-import-directory-table-in-c

/*Convert Virtual Address to File Offset */
DWORD Rva2Offset(DWORD rva, PIMAGE_SECTION_HEADER psh, PIMAGE_NT_HEADERS pnt)
{
    size_t i = 0;
    PIMAGE_SECTION_HEADER pSeh;
    if (rva == 0)
    {
        return (rva);
    }
    pSeh = psh;
    for (i = 0; i < pnt->FileHeader.NumberOfSections; i++)
    {
        if (rva >= pSeh->VirtualAddress && rva < pSeh->VirtualAddress +
            pSeh->Misc.VirtualSize)
        {
            break;
        }
        pSeh++;
    }
    return (rva - pSeh->VirtualAddress + pSeh->PointerToRawData);
}

bool validateDLL(const std::wstring& dllFilePath, const std::vector<std::wstring>& utf16DependeciesDirectories, std::map<std::string, fs::path>& dependencies)
{
    bool dllOK = true;

    auto findStringIC = [](const std::wstring& strHaystack, const std::wstring& strNeedle)->bool
    {
        auto it = std::search(
            strHaystack.begin(), strHaystack.end(),
            strNeedle.begin(), strNeedle.end(),
            [](const wchar_t ch1, const wchar_t ch2) { return std::toupper(ch1) == std::toupper(ch2); }
        );
        return (it != strHaystack.end());
    };

#if defined(NVIGI_PRODUCTION)
    // Only check signatures on plugins, ignore dependencies since we cannot validate 3rd party libs
    if (dllFilePath.find(L"nvigi.plugin.") != std::string::npos && !nvigi::security::verifyEmbeddedSignature(dllFilePath.c_str()))
    {
        NVIGI_LOG_WARN("Failed to load plugin '%S' - missing digital signature", dllFilePath.c_str());
        return false;
    }
#endif

    HANDLE handle = CreateFileW(dllFilePath.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    DWORD byteread, size = GetFileSize(handle, NULL);
    PVOID virtualpointer = VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
    ReadFile(handle, virtualpointer, size, &byteread, NULL);
    CloseHandle(handle);
    // Get pointer to NT header
    PIMAGE_NT_HEADERS           ntheaders = (PIMAGE_NT_HEADERS)(PCHAR(virtualpointer) + PIMAGE_DOS_HEADER(virtualpointer)->e_lfanew);
    PIMAGE_SECTION_HEADER       pSech = IMAGE_FIRST_SECTION(ntheaders);//Pointer to first section header
    PIMAGE_IMPORT_DESCRIPTOR    pImportDescriptor; //Pointer to import descriptor 
    if (ntheaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size != 0)/*if size of the table is 0 - Import Table does not exist */
    {
        pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD_PTR)virtualpointer + \
            Rva2Offset(ntheaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress, pSech, ntheaders));
        LPSTR libname[256];
        size_t i = 0;
        // Walk until you reached an empty IMAGE_IMPORT_DESCRIPTOR
        while (pImportDescriptor->Name != NULL)
        {
            //Get the name of each DLL
            libname[i] = (PCHAR)((DWORD_PTR)virtualpointer + Rva2Offset(pImportDescriptor->Name, pSech, ntheaders));
            // Check if dependency is where we expect it to be (system32 or path(s) provided by the host)
            // 
            // IMPORTANT: Note that we are NOT using LoadLibrary since that is not a secure method of checking where libraries are located.
            // LoadLibrary will trigger DLLMain with attach to process/thread which can contain malicious code
            bool found = false;
            for (auto& location : utf16DependeciesDirectories) try
            {
                auto fullPath = fs::path(location) / libname[i];
                // First check if we have already seen this lib?
                // Special case(s) to ignore in general
                found = findStringIC(fullPath, L"dbgHelp.dll");
                if (!found && fs::exists(fullPath) && fs::is_regular_file(fullPath))
                {
                    found = true;
                    // Not a system lib, store it and let's check it recursively
                    dependencies[libname[i]] = location;
                    if (dllOK)
                    {
                        dllOK &= validateDLL(fullPath, utf16DependeciesDirectories, dependencies);
                    }
                }
                if (found) break;
            }
            catch (std::exception& e)
            {
                // This indicates that library is not found
                NVIGI_LOG_WARN("Exception %s", e.what());
            }
            if (!found)
            {
                // Not found in our directories, must be system lib so force loading from system32
                HMODULE hImportedModule = LoadLibraryExA(libname[i], NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (hImportedModule != NULL)
                {
                    found = true;
                    FreeLibrary(hImportedModule);
                }
            }

            if (!found)
            {
                // Failed to load so missing dependency
                dllOK = false;
                NVIGI_LOG_ERROR("Failed to load or find '%s'", libname[i]);
            }
            pImportDescriptor++; //advance to next IMAGE_IMPORT_DESCRIPTOR
            i++;
        }
    }
    if (virtualpointer)
    {
        VirtualFree(virtualpointer, size, MEM_DECOMMIT);
    }
    return dllOK;
}
#endif

static SystemCaps s_caps{};
bool getSystemCaps(nvigi::VendorId forceAdapterId, uint32_t forceArchitecture, SystemCaps* info)
{
    *info = {};

#if defined(NVIGI_WINDOWS)
    SYSTEM_POWER_STATUS powerStatus{};
    if (GetSystemPowerStatus(&powerStatus))
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-system_power_status
        //if (powerStatus.BatteryFlag != 128) // No system battery according to MS docs)
        //    info->flags |= SystemFlags::eLaptopDevice;
    }

    PFND3DKMT_ENUMADAPTERS2 pfnEnumAdapters2{};
    PFND3DKMT_QUERYADAPTERINFO pfnQueryAdapterInfo{};

    D3DKMT_ADAPTERINFO adapterInfo[kMaxNumSupportedGPUs]{};
    D3DKMT_ENUMADAPTERS2 enumAdapters2{};

    auto modGDI32 = LoadLibraryExW(L"gdi32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (modGDI32)
    {
        pfnEnumAdapters2 = (PFND3DKMT_ENUMADAPTERS2)GetProcAddress(modGDI32, "D3DKMTEnumAdapters2");
        pfnQueryAdapterInfo = (PFND3DKMT_QUERYADAPTERINFO)GetProcAddress(modGDI32, "D3DKMTQueryAdapterInfo");

        // Request adapter info from KMT
        if (pfnEnumAdapters2)
        {
            enumAdapters2.NumAdapters = kMaxNumSupportedGPUs;
            enumAdapters2.pAdapters = adapterInfo;
            HRESULT enumRes = pfnEnumAdapters2(&enumAdapters2);
            if (!NT_SUCCESS(enumRes))
            {
                if (enumRes == STATUS_BUFFER_TOO_SMALL)
                {
                    NVIGI_LOG_WARN("Enumerating up to %u adapters on a system with more than that many adapters: internal error", kMaxNumSupportedGPUs);
                }
                else
                {
                    NVIGI_LOG_WARN("Adapter enumeration has failed - cannot determine adapter capabilities; Some features may be unavailable");
                }
                // Clear everything, no adapter infos available!
                enumAdapters2 = {};
            }
        }
    }

    IDXGIFactory4* factory;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory)))
    {
        uint32_t i = 0;
        IDXGIAdapter3* adapter;
        while (factory->EnumAdapters(i++, reinterpret_cast<IDXGIAdapter**>(&adapter)) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc)))
            {
                // Intel, AMD or NVIDIA physical GPUs only
                auto vendor = (VendorId)desc.VendorId;

                if (vendor == VendorId::eNVDA || vendor == VendorId::eIntel || vendor == VendorId::eAMD)
                {
                    info->adapters[info->adapterCount] = new Adapter;
                    info->adapters[info->adapterCount]->nativeInterface = adapter;
                    info->adapters[info->adapterCount]->vendor = vendor;
                    info->adapters[info->adapterCount]->bit = 1 << info->adapterCount;
                    info->adapters[info->adapterCount]->id = desc.AdapterLuid;
                    info->adapters[info->adapterCount]->deviceId = desc.DeviceId;
                    info->adapters[info->adapterCount]->dedicatedMemoryInMB = desc.DedicatedVideoMemory / (1024 * 1024);
                    info->adapters[info->adapterCount]->description = extra::utf16ToUtf8(desc.Description).c_str();
                    info->adapterCount++;

                    if (info->adapterCount == kMaxNumSupportedGPUs) break;

                    // Check HWS for this LUID if NVDA adapter
                    if (!(info->flags & SystemFlags::eHWSchedulingEnabled) && enumAdapters2.NumAdapters > 0 && pfnQueryAdapterInfo)
                    {
                        for (uint32_t k = 0; k < enumAdapters2.NumAdapters; k++)
                        {
                            if (adapterInfo[k].AdapterLuid.HighPart == desc.AdapterLuid.HighPart &&
                                adapterInfo[k].AdapterLuid.LowPart == desc.AdapterLuid.LowPart)
                            {
                                D3DKMT_QUERYADAPTERINFO kmtInfo{};
                                kmtInfo.hAdapter = adapterInfo[k].hAdapter;
                                kmtInfo.Type = KMTQAITYPE_WDDM_2_7_CAPS;
                                D3DKMT_WDDM_2_7_CAPS data{};
                                kmtInfo.pPrivateDriverData = &data;
                                kmtInfo.PrivateDriverDataSize = sizeof(data);
                                NTSTATUS err = pfnQueryAdapterInfo(&kmtInfo);
                                if (NT_SUCCESS(err) && data.HwSchEnabled)
                                {
                                    info->flags |= SystemFlags::eHWSchedulingEnabled;
                                }
                                break;
                            }
                        }
                    }

                    // Adapter released on shutdown
                }
                else
                {
                    adapter->Release();
                }
            }

        }
        factory->Release();
    }

    {
        NvU32 nvGPUCount{};
        NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS]{};
        // Detected at least one NVDA GPU, we can use NVAPI
        if (NvAPI_EnumPhysicalGPUs(nvGPUHandle, &nvGPUCount) == NVAPI_OK)
        {
            nvGPUCount = std::min((NvU32)kMaxNumSupportedGPUs, nvGPUCount);
            NvU32 driverVersion;
            NvAPI_ShortString driverName;
            NVAPI_VALIDATE_RF(NvAPI_SYS_GetDriverAndBranchVersion(&driverVersion, driverName));
            info->driverVersion.major = driverVersion / 100;
            info->driverVersion.minor = driverVersion % 100;
            for (NvU32 gpu = 0; gpu < nvGPUCount; ++gpu)
            {
                // Find LUID for NVDA physical device
                LUID id;
                NvLogicalGpuHandle hLogicalGPU;
                NVAPI_VALIDATE_RF(NvAPI_GetLogicalGPUFromPhysicalGPU(nvGPUHandle[gpu], &hLogicalGPU));
                NV_LOGICAL_GPU_DATA lData{};
                lData.version = NV_LOGICAL_GPU_DATA_VER;
                lData.pOSAdapterId = &id;
                NVAPI_VALIDATE_RF(NvAPI_GPU_GetLogicalGpuInfo(hLogicalGPU, &lData));

                // Now find adapter by matching the LUID
                for (uint32_t i = 0; i < info->adapterCount; i++)
                {
                    if (info->adapters[i]->id.HighPart == id.HighPart && info->adapters[i]->id.LowPart == id.LowPart)
                    {
                        auto& adapter = info->adapters[i];

                        NV_GPU_ARCH_INFO archInfo;
                        archInfo.version = NV_GPU_ARCH_INFO_VER;
                        NVAPI_VALIDATE_RF(NvAPI_GPU_GetArchInfo(nvGPUHandle[gpu], &archInfo));
                        adapter->architecture = archInfo.architecture;
                        adapter->implementation = archInfo.implementation;
                        adapter->revision = archInfo.revision;
                        adapter->nvHandle = nvGPUHandle[gpu];

                        NvU32 busWidth;
                        NVAPI_VALIDATE_RF(NvAPI_GPU_GetRamBusWidth(nvGPUHandle[gpu], &busWidth));

                        // grab the boost (peak) frequencies
                        NV_GPU_CLOCK_FREQUENCIES clkFreqs = {};
                        clkFreqs.version = NV_GPU_CLOCK_FREQUENCIES_VER;
                        clkFreqs.ClockType = NV_GPU_CLOCK_FREQUENCIES_BOOST_CLOCK;
                        NVAPI_VALIDATE_RF(NvAPI_GPU_GetAllClockFrequencies(nvGPUHandle[gpu], &clkFreqs));

                        // "frequency" is in kHz
                        adapter->memoryBandwidthGBPS = (float)clkFreqs.domain[NVAPI_GPU_PUBLIC_CLOCK_MEMORY].frequency * busWidth / 8000000.0f;

                        // compute a very crude estimate of GFLOPs by assuming we can do an FMAD/clk/core
                        NvU32 coreCount;
                        NVAPI_VALIDATE_RF(NvAPI_GPU_GetGpuCoreCount(nvGPUHandle[gpu], &coreCount));
                        adapter->shaderGFLOPS = (float)clkFreqs.domain[NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS].frequency * coreCount * 2.0f / 1000000.0f;


                        NVIGI_LOG_INFO("Found adapter '%s':", adapter->description.c_str());
                        NVIGI_LOG_INFO("# LUID: %u.%u", adapter->id.HighPart, adapter->id.LowPart);
                        NVIGI_LOG_INFO("# arch: 0x%x", adapter->architecture);
                        NVIGI_LOG_INFO("# impl: 0x%x", adapter->implementation);
                        NVIGI_LOG_INFO("# rev: 0x%x", adapter->revision);
                        NVIGI_LOG_INFO("# mem GBPS: %.2f", adapter->memoryBandwidthGBPS);
                        NVIGI_LOG_INFO("# shader GFLOPS: %.2f", adapter->shaderGFLOPS);
                        NVIGI_LOG_INFO("# driver: %u.%u", info->driverVersion.major, info->driverVersion.minor);
                        break;
                    }
                }

            };
        }
        else
        {
            NVIGI_LOG_WARN("NVAPI failed to initialize, please update your driver if running on NVIDIA hardware");
        }
    }

    if (modGDI32)
    {
        FreeLibrary(modGDI32);
    }
#else
    // Linux
    auto getGPUInfo = [](const char* cmd, std::string& result)->bool
    {
        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) {
            return false;
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return true;
    };
    std::string gpuArchitecture;
    if (getGPUInfo("nvidia-smi --query-gpu=name --format=csv,noheader,nounits", gpuArchitecture))
    {
        info->adapterCount = 1;
        std::string memorySize;
        getGPUInfo("nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits", memorySize);
        std::string driverVersion;
        getGPUInfo("nvidia-smi --query-gpu=driver_version --format=csv,noheader", driverVersion);        
        gpuArchitecture = std::regex_replace(gpuArchitecture, std::regex(R"(\n)"), "");
        memorySize = std::regex_replace(memorySize, std::regex(R"(\n)"), "");
        std::regex versionRegex(R"((\d+)\.(\d+))");
        std::smatch versionMatch;
        if (std::regex_search(driverVersion, versionMatch, versionRegex) && versionMatch.size() > 2) {
            info->driverVersion.major = std::stoi(versionMatch.str(1));
            info->driverVersion.minor = std::stoi(versionMatch.str(2));
        }

        info->adapters[0] = new Adapter;

        //NV_GPU_ARCHITECTURE_GP100 = 0x00000130,
        //NV_GPU_ARCHITECTURE_GV100 = 0x00000140,
        //NV_GPU_ARCHITECTURE_GV110 = 0x00000150,
        //NV_GPU_ARCHITECTURE_TU100 = 0x00000160,
        //NV_GPU_ARCHITECTURE_GA100 = 0x00000170,
        //NV_GPU_ARCHITECTURE_AD100 = 0x00000190,
        if(gpuArchitecture.find("40") != std::string::npos)
        {
            info->adapters[0]->architecture = 0x00000190;
        }
        else if(gpuArchitecture.find("30") != std::string::npos)
        {
            info->adapters[0]->architecture = 0x00000170;
        }
        else if(gpuArchitecture.find("20") != std::string::npos)
        {
            info->adapters[0]->architecture = 0x00000160;
        }
        info->adapters[0]->vendor = nvigi::VendorId::eNVDA;
        info->adapters[0]->dedicatedMemoryInMB = std::atoll(memorySize.c_str());
        NVIGI_LOG_INFO("GPU Architecture: 0x%x - driver %s - memory size %lluMB", info->adapters[0]->architecture, extra::toStr(info->driverVersion).c_str(), info->adapters[0]->dedicatedMemoryInMB);
    }    
#endif
    if (forceAdapterId != nvigi::VendorId::eAny)
    {
        for (uint32_t i = 0; i < info->adapterCount; i++)
        {
            // Not NEEDED, but might avoid confusion when we see valid adapters in the array, but a zero count
            delete info->adapters[i];
            info->adapters[i] = {};
        }
        if (forceAdapterId == nvigi::VendorId::eNone)
        {
            info->adapterCount = 0;
            NVIGI_LOG_INFO("SIMULATING/ENFORCING ZERO GRAPHICS/COMPUTE ADAPTERS as per JSON setting");
            NVIGI_LOG_INFO("Adapter count will be forced to zero");
        }
        else
        {
            info->adapterCount = 1;
            info->adapters[0] = new nvigi::system::Adapter;
            info->adapters[0]->vendor = forceAdapterId;
            info->adapters[0]->architecture = forceArchitecture;
            NVIGI_LOG_INFO("SIMULATING single adapter (vendor=0x%04x, arch=0x%04x) as per JSON setting",
                forceAdapterId, forceArchitecture);
            NVIGI_LOG_INFO("Adapter count will be forced to 1");
        }
    }

    s_caps = *info;
    return true;
}

#ifdef NVIGI_WINDOWS
using PFun_RtlGetVersion = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
using PFun_NtSetTimerResolution = NTSTATUS(NTAPI*)(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
#endif

bool getOSVersionAndUpdateTimerResolution(SystemCaps* caps)
{
    caps->osVersion = {};
#ifdef NVIGI_LINUX
    struct utsname data;
    uname(&data);
    return true;
#else
    // In Win8, the GetVersion[Ex][AW]() functions were all deprecated in favour of using
    // other more dumbed down functions such as IsWin10OrGreater(), isWinVersionOrGreater(),
    // VerifyVersionInfo(), etc.  Unfortunately these have a couple huge issues:
    //   * they cannot retrieve the actual version information, just guess at it based on
    //     boolean return values.
    //   * in order to report anything above Win8.0, the app calling the function must be
    //     manifested as a Win10 app.
    //
    // Since we can't guarantee that all host apps will be properly manifested, we can't
    // rely on any of the new version verification functions or on GetVersionEx().
    //
    // However, all of the manifest checking is done at the kernel32 level.  If we go
    // directly to the ntdll level, we do actually get accurate information about the
    // current OS version.

    Version vKernel{}, vNT{};


    // Fist we try kernel32.dll
    TCHAR filename[file::kMaxFilePath]{};
    if (GetModuleFileName(GetModuleHandleA("kernel32.dll"), filename, file::kMaxFilePath))
    {
        DWORD verHandle{};
        DWORD verSize = GetFileVersionInfoSize(filename, &verHandle);
        if (verSize != 0)
        {
            LPSTR verData = new char[verSize];
            if (GetFileVersionInfo(filename, verHandle, verSize, verData))
            {
                LPBYTE lpBuffer{};
                UINT size{};
                if (VerQueryValueA(verData, "\\", (VOID FAR * FAR*) & lpBuffer, &size))
                {
                    if (size)
                    {
                        VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                        if (verInfo->dwSignature == 0xfeef04bd)
                        {
                            vKernel.major = (verInfo->dwProductVersionMS >> 16) & 0xffff;
                            vKernel.minor = (verInfo->dwProductVersionMS >> 0) & 0xffff;
                            vKernel.build = (verInfo->dwProductVersionLS >> 16) & 0xffff;
                        }
                    }
                }
                else
                {
                    NVIGI_LOG_ERROR("VerQueryValue failed - last error %s", std::system_category().message(GetLastError()).c_str());
                }
            }
            else
            {
                NVIGI_LOG_ERROR("GetFileVersionInfo failed - last error %s", std::system_category().message(GetLastError()).c_str());
            }
            delete[] verData;
        }
        else
        {
            NVIGI_LOG_ERROR("GetFileVersionInfoSize failed - last error %s", std::system_category().message(GetLastError()).c_str());
        }
    }
    else
    {
        NVIGI_LOG_ERROR("GetModuleFileName failed - last error %s", std::system_category().message(GetLastError()).c_str());
    }

    bool res = false;
    RTL_OSVERSIONINFOW osVer{};
    auto handle = GetModuleHandleW(L"ntdll");
    auto rtlGetVersion = reinterpret_cast<PFun_RtlGetVersion>(GetProcAddress(handle, "RtlGetVersion"));
    if (rtlGetVersion)
    {
        osVer.dwOSVersionInfoSize = sizeof(osVer);
        if (res = !rtlGetVersion(&osVer))
        {
            vNT.major = osVer.dwMajorVersion;
            vNT.minor = osVer.dwMinorVersion;
            vNT.build = osVer.dwBuildNumber;
        }
        else
        {
            NVIGI_LOG_ERROR("RtlGetVersion failed %s", std::system_category().message(GetLastError()).c_str());
        }
    }
    else if (!vKernel)
    {
        // Return false only if kernel version also failed
        NVIGI_LOG_ERROR("Failed to retrieve the RtlGetVersion() function from ntdll.");
        return false;
    }

    // Pick a higher version, rtlGetVersion reports version selected on the exe compatibility mode not the actual OS version
    if (vKernel > vNT)
    {
        NVIGI_LOG_INFO("Application running in compatibility mode - version %s", extra::toStr(vNT).c_str());
        caps->osVersion = vKernel;
    }
    else
    {
        caps->osVersion = vNT;
    }

    return res;
#endif
}

void setTimerResolution()
{
#ifdef NVIGI_WINDOWS
    auto handle = GetModuleHandleW(L"ntdll");
    auto NtSetTimerResolution = reinterpret_cast<PFun_NtSetTimerResolution>(GetProcAddress(handle, "NtSetTimerResolution"));
    if (NtSetTimerResolution)
    {
        ULONG currentRes{};
        if (!NtSetTimerResolution(5000, TRUE, &currentRes))
        {
            NVIGI_LOG_INFO("Changed high resolution timer resolution to 5000 [100 ns units]");
        }
        else
        {
            NVIGI_LOG_WARN("Failed to change high resolution timer resolution to 5000 [100 ns units]");
        }
    }
    else
    {
        NVIGI_LOG_WARN("Failed to retrieve the NtSetTimerResolution() function from ntdll.");
    }
#endif
}

void cleanup(SystemCaps* caps)
{
    for (auto& adapter : caps->adapters)
    {
        if (!adapter) continue;
#ifdef NVIGI_WINDOWS
        auto i = static_cast<IUnknown*>(adapter->nativeInterface);
        i->Release();
#endif
        delete adapter;
    }
#ifdef NVIGI_WINDOWS
#endif
}

const SystemCaps* getSystemCapsShared()
{
    return &s_caps;
}

Result getVRAMStats(uint32_t adapterIndex, VRAMUsage** _usage)
{
    if(!_usage) return kResultInvalidParameter;
    // Empty struct, set all to 0
    static VRAMUsage usage{};
    // To prevent crashes always return a pointer to an "empty" struct in case we fail down the road
    *_usage = &usage;
    assert(adapterIndex < s_caps.adapterCount);
    if (adapterIndex >= s_caps.adapterCount)
    {
        NVIGI_LOG_ERROR("Unsupported adater index %u", adapterIndex);
        return kResultInvalidParameter;
    }
#ifdef NVIGI_WINDOWS
    IDXGIAdapter3* adapter3{};
    static_cast<IUnknown*>(s_caps.adapters[adapterIndex]->nativeInterface)->QueryInterface(&adapter3);
    if (adapter3)
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
        adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo);

        usage.currentUsageMB = memInfo.CurrentUsage / 1024 / 1024;
        usage.availableToReserveMB = memInfo.AvailableForReservation / 1024 / 1024;
        usage.currentReservationMB = memInfo.CurrentReservation / 1024 / 1024;
        usage.budgetMB = memInfo.Budget / 1024 / 1024;

        NV_GPU_MEMORY_INFO_EX memoryInfo = { NV_GPU_MEMORY_INFO_EX_VER };
        if (NvAPI_GPU_GetMemoryInfoEx(NvPhysicalGpuHandle(s_caps.adapters[adapterIndex]->nvHandle), &memoryInfo) == NVAPI_OK)
        {
            usage.systemUsageMB = (memoryInfo.availableDedicatedVideoMemory - memoryInfo.curAvailableDedicatedVideoMemory) / 1024 / 1024;
        }

        adapter3->Release();
    }
    else
    {
        NVIGI_LOG_ERROR("Unable to obtain IDXGIAdapter3 for adater at index %u", adapterIndex);
        return kResultInvalidState;
    }
#else
    // Linux
    auto getGPUInfo = [](const char* cmd, std::string& result)->bool
    {
            std::array<char, 128> buffer;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
            if (!pipe) {
                return false;
            }
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
            return true;
    };
    std::string memorySize("0");
    getGPUInfo("nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits", memorySize);
    memorySize = std::regex_replace(memorySize, std::regex(R"(\n)"), "");
    usage.currentUsageMB = std::atoll(memorySize.c_str());
    usage.budgetMB = s_caps.adapters[adapterIndex]->dedicatedMemoryInMB;
#endif
    return kResultOk;
}

void setPreferenceFlags(PreferenceFlags flags)
{
    if (flags & PreferenceFlags::eDisablePrivilegeDowngrade)
    {
        NVIGI_LOG_INFO("Host opted OUT from downgrading elevated privileges");
    }
    // For now this is only Windows, leaving open to set it for Linux later on if needed
#ifdef NVIGI_WINDOWS
    s_privileges.flags = flags;
#endif
}

ISystem s_instance{};
ISystem* getInterface()
{
    if (!s_instance.getSystemCaps)
    {
        s_instance.getSystemCaps = getSystemCapsShared;
        s_instance.getVRAMStats = getVRAMStats;
        s_instance.downgradeKeyAdminPrivileges = downgradePrivileges;
        s_instance.restoreKeyAdminPrivileges = restorePrivileges;
    }
    return &s_instance;
}

}
}