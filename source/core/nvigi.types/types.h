// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstring>
#include <stdexcept>

#include "source/core/nvigi.memory/memory.h"

#if NVIGI_LINUX
#define strcpy_s(a,b,c) strcpy(a,c)
#define strcat_s(a,b,c) strcat(a,c)
#define memcpy_s(a,b,c,d) memcpy(a,c,d)
#define strncpy_s(a, b, c, d) strncpy(a, c, d)
#endif

//! Simple ABI stable alternatives for std::string and std::vector
//! 
//! Not even trying to replace STL completely, just basic functionality to allow crossing DLL boundaries and ensure backwards compatibility
//! 
//! IMPORTANT: INTERNAL USE ONLY - DO NOT INCLUDE IN PUBLIC HEADERS
//! 
namespace nvigi
{

namespace types
{

class alignas(8) string {
public:
    string() : data(nullptr), length(0) {}

    string(const char* str) 
    {
        auto mm = memory::getInterface();
        length = std::strlen(str);
        data = (char*)mm->allocate(length + 1);
        strcpy_s(data, length + 1, str);
    }

    ~string() 
    {
        memory::getInterface()->deallocate(data);
    }

    // Copy constructor
    string(const string& other) 
    {
        auto mm = memory::getInterface();
        length = other.length;
        data = length > 0 ? (char*)mm->allocate(length + 1) : nullptr;
        if (data)
        {
            strcpy_s(data, length + 1, other.data);
        }
    }

    // Copy assignment operator
    string& operator=(const string& other) 
    {
        if (this != &other) 
        {
            auto mm = memory::getInterface();
            mm->deallocate(data);
            length = other.length;
            data = length > 0 ? (char*)mm->allocate(length + 1) : nullptr;
            if (data)
            {
                strcpy_s(data, length + 1, other.data);
            }
        }
        return *this;
    }

    // Concatenation operator
    friend string operator+(const string& a, const string& b) 
    {
        auto mm = memory::getInterface();
        string result;
        result.length = a.length + b.length;
        result.data = (char*)(mm->allocate(result.length + 1));
        strcpy_s(result.data, result.length + 1, a.data);
        strcat_s(result.data, result.length + 1, b.data);
        return result;
    }

    // Concatenation operator
    string& operator+=(const string& b)
    {
        auto& a = *this;
        auto mm = memory::getInterface();
        a.length = a.length + b.length;
        auto dataTemp = (char*)(mm->allocate(a.length + 1));
        strcpy_s(dataTemp, a.length + 1, a.data);
        strcat_s(dataTemp, a.length + 1, b.data);
        mm->deallocate(a.data);
        a.data = dataTemp;
        return a;
    }

    bool operator==(const string& other) const
    {
        return std::strncmp(data, other.data, length) == 0;
    }

    // Accessor
    const char* c_str() const 
    {
        return data;
    }

    // Other methods like length, append, etc., can be added as needed.
    bool empty() const
    {
        return data == nullptr || length == 0; // "" would be data allocated but length 0
    }

    // Substring method
    string substr(size_t start, size_t len = npos) const {
        if (start > length) throw std::out_of_range("start is out of range");

        auto mm = memory::getInterface();

        size_t effectiveLength = start + len > length ? length - start : len;
        char* subStr = (char*)mm->allocate(effectiveLength + 1);
        strncpy_s(subStr, effectiveLength + 1, data + start, effectiveLength);
        subStr[effectiveLength] = '\0';
        string result(subStr);
        mm->deallocate(subStr);
        return result;
    }

    // Find method
    size_t find(const char* substring, size_t pos = 0) const 
    {
        if (pos > length) return npos;
        const char* found = std::strstr(data + pos, substring);
        return found ? static_cast<size_t>(found - data) : npos;
    }

    static const size_t npos = -1;

private:
    char* data;
    size_t length;
};

static_assert(std::is_standard_layout<string> ::value, "NVIGI structure must have standard layout");
static_assert(std::alignment_of<string>::value == 8, "NVIGI structure must have alignment of 8");

template <typename T>
class alignas(8) vector {
public:

    // Simple forward iterator
    class iterator {
    public:
        iterator(T* _ptr) : ptr(_ptr) {}

        iterator& operator++() {
            ++ptr;
            return *this;
        }

        bool operator!=(const iterator& other) const {
            return ptr != other.ptr;
        }

        T& operator*() const {
            return *ptr;
        }

        T& operator*() {
            return *ptr;
        }

    private:
        T* ptr;
    };

    vector() : data_(nullptr), size_(0), capacity(0) {}
    vector(std::initializer_list<T> initList)
        : size_(initList.size()), capacity(initList.size())
    {
        auto mm = memory::getInterface();
        data_ = (T*)mm->allocate(sizeof(T) * capacity);
        std::copy(initList.begin(), initList.end(), data_);
    }

    // Constructor that takes two pointers as a range
    vector(T* start, T* end) 
    {
        auto mm = memory::getInterface();
        size_ = std::distance(start, end);
        capacity = size_;
        data_ = (T*)mm->allocate(sizeof(T) * capacity);
        std::copy(start, end, data_);
    }

    // Constructor that takes two pointers as a range
    vector(size_t sz)
    {
        auto mm = memory::getInterface();
        size_ = sz;
        capacity = sz;
        data_ = (T*)mm->allocate(sizeof(T) * capacity);
    }

    ~vector() 
    {
        clear();
    }

    // Copy constructor
    vector(const vector& other) {
        operator=(other);
    }

    // Copy assignment operator
    vector& operator=(const vector& other) 
    {
        if (this != &other) {
            auto mm = memory::getInterface();
            mm->deallocate(data_);
            data_ = (T*)mm->allocate(sizeof(T) * other.capacity);
            size_ = other.size_;
            capacity = other.capacity;
            for (size_t i = 0; i < other.size_; ++i) {
                data_[i] = other.data_[i];
            }
        }
        return *this;
    }

    // Add an element to the end of the vector
    void push_back(const T& value) {
        if (size_ == capacity) {
            expand((capacity == 0) ? 1 : 2 * capacity);
        }
        data_[size_] = value;
        ++size_;
    }

    // Access elements
    T& operator[](size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    const T& operator[](size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    // Empty
    bool empty() const
    {
        return size_ == 0 && data_ == nullptr;
    }

    // Get the size of the vector
    size_t size() const {
        return size_;
    }

    // Begin iterator
    iterator begin() const {
        return iterator(data_);
    }

    // End iterator
    iterator end() const {
        return iterator(data_ + size_);
    }

    // Begin iterator
    iterator begin() {
        return iterator(data_);
    }

    // End iterator
    iterator end() {
        return iterator(data_ + size_);
    }

    T* data() { return data_; }
    const T* data() const { return data_; }

    void resize(size_t newSize)
    {
        auto mm = memory::getInterface();
        T* newData = (T*)mm->allocate(sizeof(T) * newSize);
        for (size_t i = 0; i < std::min(newSize,size_); ++i) {
            newData[i] = data_[i];
        }
        for (size_t i = size_; i < newSize; ++i) {
            newData[i] = {};
        }
        mm->deallocate(data_);
        data_ = newData;
        capacity = newSize;
        size_ = newSize;
    }

    void reserve(size_t newCapacity)
    {
        auto mm = memory::getInterface();
        T* newData = (T*)mm->allocate(sizeof(T) * newCapacity);
        for (size_t i = 0; i < std::min(newCapacity, size_); ++i) {
            newData[i] = data_[i];
        }
        mm->deallocate(data_);
        data_ = newData;
        capacity = newCapacity;
    }

    inline iterator find(const T& v) const
    {
        for (size_t i = 0; i < size_; i++)
        {
            if (data_[i] == v) return iterator(data_ + i);
        }
        return end();
    }

    inline bool contains(const T& v) const
    {
        return find(v) != end();
    }

    inline void clear()
    {
        for (size_t i = 0; i < size_; i++)
        {
            data_[i].~T();
        }
        auto mm = memory::getInterface();
        mm->deallocate(data_);
        data_ = nullptr;
        size_ = 0;
        capacity = 0;
    }

private:
    T* data_{};
    size_t size_{};
    size_t capacity{};

    void expand(size_t newCapacity)
    {
        auto mm = memory::getInterface();
        T* newData = (T*)mm->allocate(sizeof(T) * newCapacity);
        for (size_t i = 0; i < size_; ++i) {
            newData[i] = data_[i];
        }
        for (size_t i = size_; i < newCapacity; ++i) {
            newData[i] = {};
        }
        for (size_t i = 0; i < size_; i++)
        {
            data_[i].~T();
        }
        mm->deallocate(data_);
        data_ = newData;
        capacity = newCapacity;
    }
};

}
}