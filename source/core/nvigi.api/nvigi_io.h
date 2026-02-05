// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "nvigi_struct.h"
#include "nvigi_result.h"

namespace nvigi
{

//! Access mode for memory mappings
//! 
//! Specifies the intended access pattern for a mapped region.
enum class MapAccess : uint32_t
{
    //! Caller will only read from the mapping
    eReadOnly  = 0,
    //! Caller will only write to the mapping (useful for streaming uploads)
    eWriteOnly = 1,
    //! Caller will both read from and write to the mapping
    eReadWrite = 2,
};

//! Interface 'FileIOCallbacks'
//!
//! {BABCD7BC-4951-4DCD-BF23-18D5FF6120DD}
struct alignas(8) FileIOCallbacks
{
    FileIOCallbacks() 
        : userData(nullptr)
        , open(nullptr)
        , close(nullptr)
        , size(nullptr)
        , tell(nullptr)
        , seek(nullptr)
        , read(nullptr)
        , write(nullptr)
        , map(nullptr)
        , unmap(nullptr)
        , getLastError(nullptr)
    { }

    FileIOCallbacks(void *ud,
                    void *(*o)(void *, const char *, const char *),
                    void (*c)(void *, void *),
                    size_t (*sz)(void *, void *),
                    size_t (*t)(void *, void *),
                    int (*sk)(void *, void *, size_t, int),
                    size_t (*r)(void *, void *, void *, size_t),
                    size_t (*w)(void *, void *, const void *, size_t),
                    uint8_t* (*m)(void*, void*, size_t, size_t, MapAccess) = nullptr,
                    void (*um)(void*, void*, uint8_t*) = nullptr,
                    Result (*gle)(void*, void*) = nullptr)
        : userData(ud)
        , open(o)
        , close(c)
        , size(sz)
        , tell(t)
        , seek(sk)
        , read(r)
        , write(w)
        , map(m)
        , unmap(um)
        , getLastError(gle)
    { }

    NVIGI_UID(UID({0xbabcd7bc, 0x4951, 0x4dcd,{0xbf, 0x23, 0x18, 0xd5, 0xff, 0x61, 0x20, 0xdd}}), kStructVersion1)

    // User data pointer passed to all callbacks
    void * userData;
    
    //! Open a file/memory source. Returns a handle (pointer) that will be passed to other callbacks.
    //! fname: path/identifier for the model file
    //! mode: file mode (e.g., "rb")
    //! 
    //! NOTE: Must implement "rb" (read binary) at minimum and if 'write' callback is provided also "wb" (write binary)
    //! 
    //! Returns: handle on success, NULL on failure (call getLastError for details)
    void * (*open)(void * userData, const char * fname, const char * mode);
    
    //! Close the handle opened by open callback
    void (*close)(void * userData, void * handle);
    
    //! Get the total size of the data source
    //! 
    //! Returns: size in bytes
    size_t (*size)(void * userData, void * handle);
    
    //! Get current position in the data source
    //! 
    //! Returns: current position in bytes
    size_t (*tell)(void * userData, void * handle);
    
    //! Seek to a position in the data source
    //! offset: offset in bytes
    //! whence: SEEK_SET, SEEK_CUR, or SEEK_END
    //! 
    //! Returns: 0 on success, non-zero on error (call getLastError for details)
    int (*seek)(void * userData, void * handle, size_t offset, int whence);
    
    //! Read data from the data source
    //! ptr: buffer to read into
    //! len: number of bytes to read
    //! 
    //! Returns: number of bytes actually read (call getLastError if less than expected)
    size_t (*read)(void * userData, void * handle, void * ptr, size_t len);
    
    //! Write data to the data source
    //! ptr: buffer to write from
    //! len: number of bytes to write
    //! 
    //! NOTE: This is optional, can be NULL if writing is not supported
    //! 
    //! Returns: number of bytes actually written (call getLastError if less than expected)
    size_t (*write)(void * userData, void * handle, const void * ptr, size_t len);

    //! Map data to memory (optional)
    //! offset: offset in bytes from the start of the data source
    //! size: size of data to map in bytes
    //! access: read/write access mode for the mapping
    //!
    //! NOTE: This is optional, can be NULL if mapping is not supported.
    //! Useful when the data source is already in memory (e.g., memory-mapped files,
    //! embedded data, or pre-loaded buffers) to avoid unnecessary copies.
    //!
    //! Returns: pointer to mapped data on success, nullptr on failure (call getLastError for details)
    uint8_t* (*map)(void* userData, void* handle, size_t offset, size_t size, MapAccess access);

    //! Unmap data from memory (optional)
    //! mappedPtr: pointer previously returned by map callback
    //!
    //! NOTE: This is optional, can be NULL if mapping is not supported.
    //! Must be called for each successful map() call to release resources.
    void (*unmap)(void* userData, void* handle, uint8_t* mappedPtr);

    //! Get the last error code for a handle (optional, like errno)
    //! handle: the handle to query, or NULL to get the last global error (e.g., from failed open)
    //!
    //! NOTE: This is optional, can be NULL if detailed error reporting is not needed.
    //! The error is set by the last operation that failed on the given handle.
    //! Successful operations may or may not clear the error (implementation-defined).
    //!
    //! Returns: kResultOk if no error, or an error code describing the last failure:
    //!          - kResultInvalidParameter: invalid parameters passed to an operation
    //!          - kResultItemNotFound: file/resource not found (open failed)
    //!          - kResultInsufficientResources: out of memory
    //!          - kResultNoImplementation: operation not supported
    //!          - kResultInvalidState: operation not valid in current state
    //!          - etc.
    Result (*getLastError)(void* userData, void* handle);

    //! v2+ members go here, remember to update the kStructVersionN in the above NVIGI_UID macro!
};

NVIGI_VALIDATE_STRUCT(FileIOCallbacks)

} // namespace nvigi
