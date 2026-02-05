// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

//! POSIX errno-style common error codes
constexpr uint32_t kResultPermissionDenied = 19 << 24;     //!< EACCES - Permission denied
constexpr uint32_t kResultAlreadyExists = 20 << 24;        //!< EEXIST - File/resource already exists
constexpr uint32_t kResultIOError = 21 << 24;              //!< EIO - Input/output error
constexpr uint32_t kResultNoSpace = 22 << 24;              //!< ENOSPC - No space left on device
constexpr uint32_t kResultReadOnly = 23 << 24;             //!< EROFS - Read-only file system/resource
constexpr uint32_t kResultBusy = 24 << 24;                 //!< EBUSY - Resource busy
constexpr uint32_t kResultOutOfRange = 25 << 24;           //!< ERANGE - Value out of range
constexpr uint32_t kResultWouldBlock = 26 << 24;           //!< EAGAIN - Resource temporarily unavailable, try again
constexpr uint32_t kResultInterrupted = 27 << 24;          //!< EINTR - Operation interrupted
constexpr uint32_t kResultCanceled = 28 << 24;             //!< ECANCELED - Operation canceled
constexpr uint32_t kResultConnectionRefused = 29 << 24;    //!< ECONNREFUSED - Connection refused
constexpr uint32_t kResultNetworkUnreachable = 30 << 24;   //!< ENETUNREACH - Network unreachable
constexpr uint32_t kResultEndOfFile = 31 << 24;            //!< EOF - End of file/stream reached

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
        case 19: return "PermissionDenied";
        case 20: return "AlreadyExists";
        case 21: return "IOError";
        case 22: return "NoSpace";
        case 23: return "ReadOnly";
        case 24: return "Busy";
        case 25: return "OutOfRange";
        case 26: return "WouldBlock";
        case 27: return "Interrupted";
        case 28: return "Canceled";
        case 29: return "ConnectionRefused";
        case 30: return "NetworkUnreachable";
        case 31: return "EndOfFile";
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
        case 19: return "Permission denied. The operation requires elevated privileges or access rights";
        case 20: return "The file or resource already exists";
        case 21: return "An input/output error occurred during the operation";
        case 22: return "No space left on device or storage quota exceeded";
        case 23: return "The file system or resource is read-only";
        case 24: return "The resource is currently busy or locked by another operation";
        case 25: return "A value is out of the valid range or would cause overflow";
        case 26: return "The operation would block. Resource temporarily unavailable, try again";
        case 27: return "The operation was interrupted before completion";
        case 28: return "The operation was canceled by request";
        case 29: return "Connection refused by the remote host";
        case 30: return "The network is unreachable";
        case 31: return "End of file or stream reached";
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
