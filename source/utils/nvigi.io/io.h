// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <errno.h>
#include <cstdio>

#include "nvigi_io.h"

namespace nvigi
{

// Thread-local storage for errors that occur before we have a handle (e.g., open failures)
static thread_local Result g_memoryIOLastError = kResultOk;

// Example: Simple memory buffer wrapper
struct MemoryBuffer {
    std::vector<uint8_t> data;
    size_t position;
    std::string identifier;
    Result lastError;

    MemoryBuffer() : position(0), lastError(kResultOk) {}
};

//! Fake memory-based file IO callbacks implementation to provide an example
//! 
//! Here we just use standard file IO to load the file into memory 
//! In real scenario, this could be loading from a database, network, etc.

static void* memory_open(void* user_data, const char* fname, const char* mode) {
    (void)user_data;

    NVIGI_LOG_TEST_INFO("opening model from memory: %s", fname);

    if (std::string(mode) != "rb") {
        NVIGI_LOG_TEST_ERROR("only 'rb' mode is supported in this example");
        g_memoryIOLastError = kResultNoImplementation;
        return nullptr;
    }

    std::ifstream file(fname, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        NVIGI_LOG_TEST_ERROR("failed to open file: %s", fname);
        g_memoryIOLastError = kResultItemNotFound;
        return nullptr;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    MemoryBuffer* buffer = new (std::nothrow) MemoryBuffer();
    if (!buffer) {
        g_memoryIOLastError = kResultInsufficientResources;
        return nullptr;
    }
    
    try {
        buffer->data.resize(size);
    } catch (...) {
        delete buffer;
        g_memoryIOLastError = kResultInsufficientResources;
        return nullptr;
    }
    
    buffer->identifier = fname;

    if (!file.read(reinterpret_cast<char*>(buffer->data.data()), size)) {
        NVIGI_LOG_TEST_ERROR("failed to read file: %s", fname);
        delete buffer;
        g_memoryIOLastError = kResultIOError;
        return nullptr;
    }

    NVIGI_LOG_TEST_INFO("loaded %lld bytes into memory", static_cast<long long>(size));

    g_memoryIOLastError = kResultOk;
    return buffer;
}

static void memory_close(void* user_data, void* handle) {
    (void)user_data;
    MemoryBuffer* buffer = static_cast<MemoryBuffer*>(handle);
    NVIGI_LOG_TEST_INFO("closing memory buffer : %s", buffer->identifier.c_str());
    delete buffer;
}

static size_t memory_size(void* user_data, void* handle) {
    (void)user_data;
    MemoryBuffer* buffer = static_cast<MemoryBuffer*>(handle);
    return buffer->data.size();
}

static size_t memory_tell(void* user_data, void* handle) {
    (void)user_data;
    MemoryBuffer* buffer = static_cast<MemoryBuffer*>(handle);
    return buffer->position;
}

static int memory_seek(void* user_data, void* handle, size_t offset, int whence) {
    (void)user_data;
    MemoryBuffer* buffer = static_cast<MemoryBuffer*>(handle);

    size_t new_position = 0;

    switch (whence) {
    case SEEK_SET:
        new_position = offset;
        break;
    case SEEK_CUR:
        new_position = buffer->position + offset;
        break;
    case SEEK_END:
        new_position = buffer->data.size() + offset;
        break;
    default:
        buffer->lastError = kResultInvalidParameter;
        return EINVAL;
    }

    if (new_position > buffer->data.size()) {
        buffer->lastError = kResultOutOfRange;
        return EOVERFLOW;
    }

    buffer->position = new_position;
    buffer->lastError = kResultOk;
    return 0;
}

static size_t memory_read(void* user_data, void* handle, void* ptr, size_t len) {
    (void)user_data;
    MemoryBuffer* buffer = static_cast<MemoryBuffer*>(handle);

    size_t bytes_to_read = std::min(len, buffer->data.size() - buffer->position);

    if (bytes_to_read > 0) {
        std::memcpy(ptr, buffer->data.data() + buffer->position, bytes_to_read);
        buffer->position += bytes_to_read;
        buffer->lastError = kResultOk;
    } else if (len > 0) {
        // Tried to read but we're at end of file
        buffer->lastError = kResultEndOfFile;
    } else {
        buffer->lastError = kResultOk;
    }

    return bytes_to_read;
}

static uint8_t* memory_map(void* user_data, void* handle, size_t offset, size_t size, MapAccess access) {
    (void)user_data;
    (void)access; // Memory buffer supports all access modes
    
    if (!handle) {
        g_memoryIOLastError = kResultInvalidParameter;
        return nullptr;
    }

    MemoryBuffer* buffer = static_cast<MemoryBuffer*>(handle);

    if (offset + size > buffer->data.size()) {
        buffer->lastError = kResultOutOfRange;
        return nullptr;
    }

    buffer->lastError = kResultOk;
    return buffer->data.data() + offset;
}

static void memory_unmap(void* user_data, void* handle, uint8_t* mappedPtr) {
    (void)user_data;
    (void)mappedPtr;
    // In this simple example, unmap does nothing since the memory is owned by MemoryBuffer
    if (handle) {
        static_cast<MemoryBuffer*>(handle)->lastError = kResultOk;
    }
}

static Result memory_getLastError(void* user_data, void* handle) {
    (void)user_data;
    if (handle) {
        return static_cast<MemoryBuffer*>(handle)->lastError;
    }
    // No handle - return the global error (e.g., from failed open)
    return g_memoryIOLastError;
}

} // namespace nvigi
