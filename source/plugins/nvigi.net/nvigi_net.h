// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

namespace nvigi
{
namespace plugin
{
namespace net
{
constexpr PluginID kId = { {0xb73ed870, 0x8091, 0x491e,{0xa4, 0x9b, 0x6e, 0x19, 0x8f, 0xe9, 0x0e, 0x2c}}, 0x544a60 };  // {B73ED870-8091-491E-A49B-6E198FE90E2C} [nvigi.plugin.net]
}
}

namespace net
{

constexpr uint32_t kResultNetMissingAuthentication = 1 << 24 | plugin::net::kId.crc24;
constexpr uint32_t kResultNetFailedToInitializeCurl = 2 << 24 | plugin::net::kId.crc24;
constexpr uint32_t kResultNetCurlError = 3 << 24 | plugin::net::kId.crc24;
constexpr uint32_t kResultNetServerError = 4 << 24 | plugin::net::kId.crc24;
constexpr uint32_t kResultNetTimeout = 5 << 24 | plugin::net::kId.crc24;
constexpr uint32_t kResultNetResponseTooLarge = 6 << 24 | plugin::net::kId.crc24;

}
}