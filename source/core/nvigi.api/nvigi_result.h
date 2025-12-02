// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
constexpr uint32_t kResultTimedOut = 18 << 24;

//! Convert a result code to a human-readable string name
//!
//! @param result The result code to convert
//! @return A null-terminated string containing the result name (e.g., "Ok", "DriverOutOfDate")
//!         Returns "Unknown" for unrecognized result codes
inline const char* resultToString(Result result)
{
    // Extract the error code (top 8 bits) and compare
    uint32_t errorCode = result >> 24;
    
    switch (errorCode)
    {
        case 0:  return "Ok";
        case 1:  return "DriverOutOfDate";
        case 2:  return "OSOutOfDate";
        case 3:  return "NoPluginsFound";
        case 4:  return "InvalidParameter";
        case 5:  return "NoSupportedHardwareFound";
        case 6:  return "MissingInterface";
        case 7:  return "MissingDynamicLibraryDependency";
        case 8:  return "InvalidState";
        case 9:  return "Exception";
        case 10: return "JSONException";
        case 11: return "RPCError";
        case 12: return "InsufficientResources";
        case 13: return "NotReady";
        case 14: return "PluginOutOfDate";
        case 15: return "DuplicatedPluginId";
        case 16: return "NoImplementation";
        case 17: return "ItemNotFound";
        case 18: return "TimedOut";
        default: return "Unknown";
    }
}

//! Convert a result code to a detailed explanation string
//!
//! @param result The result code to convert
//! @return A null-terminated string containing a detailed explanation of the result code
//!         Returns "Unknown result code" for unrecognized result codes
inline const char* resultToExplanation(Result result)
{
    // Extract the error code (top 8 bits) and compare
    uint32_t errorCode = result >> 24;
    
    switch (errorCode)
    {
        case 0:  return "Operation completed successfully";
        case 1:  return "Graphics driver is out of date and needs to be updated to a newer version";
        case 2:  return "Operating system is out of date and needs to be updated";
        case 3:  return "No plugins were found during initialization. Check plugin installation paths";
        case 4:  return "One or more parameters passed to the function are invalid or out of range";
        case 5:  return "No compatible hardware was detected on this system";
        case 6:  return "A required interface is missing. The plugin may not be properly initialized";
        case 7:  return "A required dynamic library dependency (DLL/SO) could not be loaded";
        case 8:  return "The operation cannot be performed in the current state. Check operation order";
        case 9:  return "An unexpected exception occurred during operation";
        case 10: return "An error occurred while parsing or processing JSON data";
        case 11: return "An error occurred during remote procedure call (RPC) communication";
        case 12: return "Insufficient system resources (memory, GPU resources, etc.) to complete the operation";
        case 13: return "The requested resource or operation is not ready yet. Try again later";
        case 14: return "Plugin is out of date and needs to be updated to a compatible version";
        case 15: return "A plugin with the same ID has already been registered";
        case 16: return "The requested functionality has not been implemented yet";
        case 17: return "The requested item or resource was not found";
        case 18: return "The operation did not complete within the specified timeout period";
        default: return "Unknown result code. This may be a plugin-specific error";
    }
}

//! Check if a result code indicates success
//!
//! @param result The result code to check
//! @return true if the operation was successful, false otherwise
inline bool resultIsSuccess(Result result)
{
    return result == kResultOk;
}

//! Check if a result code indicates an error
//!
//! @param result The result code to check
//! @return true if the operation failed, false otherwise
inline bool resultIsError(Result result)
{
    return result != kResultOk;
}

}
