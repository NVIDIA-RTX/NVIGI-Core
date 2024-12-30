// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <filesystem>
#include <inttypes.h>
#include <map>

#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.api/nvigi_struct.h"
#include "source/core/nvigi.api/nvigi_version.h"
#include "source/core/nvigi.api/nvigi_types.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.types/types.h"

namespace fs = std::filesystem;

namespace nvigi
{

namespace system
{

constexpr uint32_t kMaxNumSupportedGPUs = 8;

// {71423331-2534-4C23-A4B1-95D54CC25F4F}
struct alignas(8) Adapter {
    Adapter() {}; 
    NVIGI_UID(UID({ 0x71423331, 0x2534, 0x4c23,{ 0xa4, 0xb1, 0x95, 0xd5, 0x4c, 0xc2, 0x5f, 0x4f } }), kStructVersion1)
    LUID id {};
    VendorId vendor{};
    uint32_t bit; // in the adapter bit-mask
    uint32_t architecture{};
    uint32_t implementation{};
    uint32_t revision{};
    uint32_t deviceId{};
    size_t dedicatedMemoryInMB{};
    float memoryBandwidthGBPS{};
    float shaderGFLOPS{};
    void* nativeInterface{};
    types::string description{};
    void* nvHandle{};

    // NEW MEMBERS GO HERE, BUMP THE VERSION!
};

// {18901866-8FFF-4313-8EAB-A5B5F3756811}
struct alignas(8) SystemCaps {
    SystemCaps() {}; 
    NVIGI_UID(UID({ 0x18901866, 0x8fff, 0x4313,{ 0x8e, 0xab, 0xa5, 0xb5, 0xf3, 0x75, 0x68, 0x11 } }), kStructVersion1)
    uint32_t adapterCount {};
    Version osVersion{};
    Version driverVersion{};
    Adapter* adapters[kMaxNumSupportedGPUs]{};
    SystemFlags flags{};

    // NEW MEMBERS GO HERE, BUMP THE VERSION!
};

//! Interface 'VRAMUsage'
//!
//! {BCB847BC-E8D0-466D-927A-C810A7A239BC}
struct alignas(8) VRAMUsage
{
    VRAMUsage() { };
    NVIGI_UID(UID({ 0xbcb847bc, 0xe8d0, 0x466d,{0x92, 0x7a, 0xc8, 0x10, 0xa7, 0xa2, 0x39, 0xbc} }), kStructVersion1)

    size_t currentUsageMB;          // this process
    size_t systemUsageMB;           // system wide usage
    size_t availableToReserveMB;
    size_t currentReservationMB;
    size_t budgetMB;                // total budget

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(VRAMUsage)

//! Interface 'ISystem'
//!
//! {E2B94F2B-7AE8-467D-98E0-6F2B14410079}
struct alignas(8) ISystem
{
    ISystem() { };
    NVIGI_UID(UID({ 0xe2b94f2b, 0x7ae8, 0x467d,{0x98, 0xe0, 0x6f, 0x2b, 0x14, 0x41, 0x00, 0x79} }), kStructVersion1)

    const SystemCaps* (*getSystemCaps)() {};
    Result (*getVRAMStats)(uint32_t adapterIndex, VRAMUsage** usage);
    Result (*downgradeKeyAdminPrivileges)();
    Result (*restoreKeyAdminPrivileges)();
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(ISystem)

ISystem* getInterface();

#ifdef NVIGI_WINDOWS

struct ScopedDowngradePrivileges
{
    ScopedDowngradePrivileges() { nvigi::system::getInterface()->downgradeKeyAdminPrivileges(); }
    ~ScopedDowngradePrivileges() { nvigi::system::getInterface()->restoreKeyAdminPrivileges(); }
};

#endif

}
}
