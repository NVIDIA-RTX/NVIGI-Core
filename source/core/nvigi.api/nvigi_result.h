// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

namespace nvigi
{

using Result = uint32_t;

//! Common results
//!
//! IMPORTANT: Interfaces should define custom error codes in their respective headers
//! 
//! The following formatting should be used:
//! 
//!     32bit RESULT VALUE
//! | 31 ... 24  | 23 ....  0 |
//! | ERROR CODE | FEATURE ID |
//! 
//! constexpr uint32_t kResultMyResult = (myErrorCode << 24) | nvigi::plugin::$NAME::$BACKEND::$API::kId.crc24; // crc24 is auto generated and part of the PluginID
//!
//! Here is an example from "source\plugins\nvigi.net\nvigi_net.h"
//! 
//! constexpr uint32_t kResultNetMissingAuthentication = 1 << 24 | plugin::net::kId.crc24;

//! Up to 256 error codes, bottom 24bits are reserved and must be 0 since these are common error codes
//! 
constexpr uint32_t kResultOk = 0;
constexpr uint32_t kResultDriverOutOfDate = 1 << 24;
constexpr uint32_t kResultOSOutOfDate = 2 << 24;
constexpr uint32_t kResultNoPluginsFound = 3 << 24;
constexpr uint32_t kResultInvalidParameter = 4 << 24;
constexpr uint32_t kResultNoSupportedHardwareFound = 5 << 24;
constexpr uint32_t kResultMissingInterface = 6 << 24;
constexpr uint32_t kResultMissingDynamicLibraryDependency = 7 << 24;
constexpr uint32_t kResultInvalidState = 8 << 24;
constexpr uint32_t kResultException = 9 << 24;
constexpr uint32_t kResultJSONException = 10 << 24;
constexpr uint32_t kResultRPCError = 11 << 24;
constexpr uint32_t kResultInsufficientResources = 12 << 24;
constexpr uint32_t kResultNotReady = 13 << 24;
constexpr uint32_t kResultPluginOutOfDate = 14 << 24;
constexpr uint32_t kResultDuplicatedPluginId = 15 << 24;
constexpr uint32_t kResultNoImplementation = 16 << 24;
constexpr uint32_t kResultItemNotFound = 17 << 24;

}
