// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

namespace nvigi
{
//! Avoiding system includes with INT_MAX and FLT_MAX
constexpr float kUnassignedF = 3.402823466e+38F;
constexpr int kUnassignedI = 2147483647;
}

#define NVIGI_ENUM_OPERATORS_64(T)                                                      \
inline bool operator&(T a, T b)                                                         \
{                                                                                       \
    return ((uint64_t)a & (uint64_t)b) != 0;                                            \
}                                                                                       \
                                                                                        \
inline T& operator&=(T& a, T b)                                                         \
{                                                                                       \
    a = (T)((uint64_t)a & (uint64_t)b);                                                 \
    return a;                                                                           \
}                                                                                       \
                                                                                        \
inline T operator|(T a, T b)                                                            \
{                                                                                       \
    return (T)((uint64_t)a | (uint64_t)b);                                              \
}                                                                                       \
                                                                                        \
inline T& operator |= (T& lhs, T rhs)                                                   \
{                                                                                       \
    lhs = (T)((uint64_t)lhs | (uint64_t)rhs);                                           \
    return lhs;                                                                         \
}                                                                                       \
                                                                                        \
inline T operator~(T a)                                                                 \
{                                                                                       \
    return (T)~((uint64_t)a);                                                           \
}

#define NVIGI_ENUM_OPERATORS_32(T)                                                      \
inline bool operator&(T a, T b)                                                         \
{                                                                                       \
    return ((uint32_t)a & (uint32_t)b) != 0;                                            \
}                                                                                       \
                                                                                        \
inline T& operator&=(T& a, T b)                                                         \
{                                                                                       \
    a = (T)((uint32_t)a & (uint32_t)b);                                                 \
    return a;                                                                           \
}                                                                                       \
                                                                                        \
inline T operator|(T a, T b)                                                            \
{                                                                                       \
    return (T)((uint32_t)a | (uint32_t)b);                                              \
}                                                                                       \
                                                                                        \
inline T& operator |= (T& lhs, T rhs)                                                   \
{                                                                                       \
    lhs = (T)((uint32_t)lhs | (uint32_t)rhs);                                           \
    return lhs;                                                                         \
}                                                                                       \
                                                                                        \
inline T operator~(T a)                                                                 \
{                                                                                       \
    return (T)~((uint32_t)a);                                                           \
}

