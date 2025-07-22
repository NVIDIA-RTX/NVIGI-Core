// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <filesystem>
#include <string>
#include <stdexcept>
#include <vector>
#ifdef NVIGI_WINDOWS
#include <windows.h> // For GetTempPathW
#include <rpc.h>     // For UUID generation
#pragma comment(lib, "rpcrt4.lib") // Link RPC library
#endif

namespace fs = std::filesystem;

#include "source/utils/nvigi.ai/nvigi_stl_helpers.h"

namespace nvigi::stl
{

constexpr const char* kTestInputSlotA = "TestInputSlotA";
constexpr const char* kTestInputSlotBA = "TestInputSlotB";

// Write unit tests for STL helpers
TEST_CASE("InferenceDataTextSTLHelper", "[stl][text]")
{
    // Test constructors and operators for nvigi::InferenceDataTextSTLHelper
    nvigi::InferenceDataTextSTLHelper textHelper;
    nvigi::InferenceDataText* slot = textHelper;
    textHelper = "Hello World!";
    REQUIRE(strcmp(slot->getUTF8Text(), "Hello World!") == 0);

    std::vector<nvigi::InferenceDataSlot> inputs =
    {
        {kTestInputSlotA, textHelper},
    };
    textHelper = "Another Hello!";
    auto textData = castTo<nvigi::InferenceDataText>(inputs[0].data);
    REQUIRE(textData != nullptr);
    REQUIRE(strcmp(textData->getUTF8Text(), "Another Hello!") == 0);
}

TEST_CASE("InferenceDataAudioSTLHelper", "[stl][audio]")
{
    // Test constructors and operators for nvigi::InferenceDataAudioSTLHelper
    nvigi::InferenceDataAudioSTLHelper audioHelper;
    nvigi::InferenceDataAudio* slot = audioHelper;
    std::vector<int16_t> pcm16 = { 0, 1, 2, 3, 4, 5 };
    audioHelper = pcm16;
    REQUIRE(slot->bitsPerSample == 16);
    REQUIRE(slot->channels == 1);
    REQUIRE(slot->dataType == nvigi::AudioDataType::ePCM);
    std::vector<nvigi::InferenceDataSlot> inputs =
    {
        {kTestInputSlotBA, audioHelper},
    };
    audioHelper = std::vector<int16_t>{ 5, 4, 3, 2, 1, 0 };
    auto audioData = castTo<nvigi::InferenceDataAudio>(inputs[0].data);
    REQUIRE(audioData != nullptr);
    REQUIRE(audioData->bitsPerSample == 16);
    REQUIRE(audioData->channels == 1);
    REQUIRE(audioData->dataType == nvigi::AudioDataType::ePCM);
}

TEST_CASE("InferenceDataByteArraySTLHelper", "[stl][bytearray]")
{
    // Test constructors and operators for nvigi::InferenceDataByteArraySTLHelper
    nvigi::InferenceDataByteArraySTLHelper byteArrayHelper;
    nvigi::InferenceDataByteArray* slot = byteArrayHelper;
    byteArrayHelper = { 0x01, 0x02, 0x03, 0x04 };
    auto data = castTo<CpuData>(slot->bytes);
    REQUIRE(data != nullptr);
    REQUIRE(data->sizeInBytes == 4);
    REQUIRE(((const uint8_t*)data->buffer)[0] == 0x01); 
    REQUIRE(((const uint8_t*)data->buffer)[1] == 0x02);
    REQUIRE(((const uint8_t*)data->buffer)[2] == 0x03);
    REQUIRE(((const uint8_t*)data->buffer)[3] == 0x04);
    std::vector<nvigi::InferenceDataSlot> inputs =
    {
        {kTestInputSlotBA, byteArrayHelper},
    };
    byteArrayHelper = { 0x04, 0x03, 0x02, 0x01 };
    auto byteArrayData = castTo<nvigi::InferenceDataByteArray>(inputs[0].data);
    REQUIRE(byteArrayData != nullptr);
    auto data2 = castTo<CpuData>(byteArrayData->bytes);
    REQUIRE(data2 != nullptr);
    REQUIRE(data2->sizeInBytes == 4);
    REQUIRE(((const uint8_t*)data2->buffer)[0] == 0x04);
    REQUIRE(((const uint8_t*)data2->buffer)[1] == 0x03);
    REQUIRE(((const uint8_t*)data2->buffer)[2] == 0x02);
    REQUIRE(((const uint8_t*)data2->buffer)[3] == 0x01);
    
    std::vector<uint8_t> byteArray = { 0x01, 0x02, 0x03, 0x04, 0x05 };
    InferenceDataByteArraySTLHelper byteArrayHelper1(byteArray.data(), byteArray.size());
    nvigi::InferenceDataByteArray* slot1 = byteArrayHelper1;
    auto data1 = castTo<CpuData>(slot1->bytes);
    REQUIRE(data1 != nullptr);
    REQUIRE(data1->sizeInBytes == 5);
    REQUIRE(((const uint8_t*)data1->buffer)[0] == 0x01);
    REQUIRE(((const uint8_t*)data1->buffer)[1] == 0x02);
    REQUIRE(((const uint8_t*)data1->buffer)[2] == 0x03);
    REQUIRE(((const uint8_t*)data1->buffer)[3] == 0x04);
    REQUIRE(((const uint8_t*)data1->buffer)[4] == 0x05);
}
} // namespace nvigi::stl

#ifdef NVIGI_WINDOWS

namespace nvigi::plugin
{
// NOTE: We are pretending to be a plugin here since we want to test model repo processing
struct Info
{
    PluginID id;
};
struct Ctx
{
    Info info;
    nvigi::framework::IFramework* framework;
};
static Ctx s_ctx;
Ctx* getContext() { return &s_ctx; }
}
#include "source/utils/nvigi.ai/ai.h"

// Structure to hold plugin parameters
struct PluginConfig {
    std::wstring name;       // e.g., "whisper", "llama"
    std::wstring backend;    // e.g., "cpu", "gpu"
    int num_plugins;         // Number of plugin directories
    int num_guids_per_plugin; // Number of GUID subdirs per plugin
};

// Generates a temp directory structure with plugins, GUIDs, and files
// Returns the root path (with \\?\ if long_path is true)
std::wstring create_plugin_temp_directory(bool long_path, const PluginConfig& config) {
    // Step 1: Get the system's temp directory
    wchar_t temp_base[MAX_PATH];
    DWORD len = GetTempPathW(MAX_PATH, temp_base);
    if (len == 0 || len > MAX_PATH) {
        throw std::runtime_error("Failed to get temp path: " + std::to_string(GetLastError()));
    }

    // Step 2: Construct the root path (long or regular)
    std::wstring root = temp_base;
    if (long_path) {
        // Add repeated segments to exceed MAX_PATH
        std::wstring long_segment = L"verylongdirectoryname1234567890"; // 30 chars
        for (int i = 0; i < 10; ++i) {
            root = fs::path(root) / long_segment;
        }
        root = L"\\\\?\\" + root;
    }

    // Step 3: Create the root directory
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) {
        throw std::runtime_error("Failed to create root directory: " + ec.message());
    }

    // Step 4: Create plugin directories
    for (int p = 0; p < config.num_plugins; ++p) {
        // Plugin name: nvigi.plugin.$name.$backend
        std::wstring plugin_name = L"nvigi.plugin." + config.name + L"." + config.backend;
        if (p > 0) {
            plugin_name += L"." + std::to_wstring(p); // Append index for uniqueness if > 1
        }
        fs::path plugin_path = fs::path(root) / plugin_name;

        // Create plugin directory
        fs::create_directory(plugin_path, ec);
        if (ec) {
            throw std::runtime_error("Failed to create plugin directory: " + ec.message());
        }

        // Step 5: Create GUID subdirectories
        for (int g = 0; g < config.num_guids_per_plugin; ++g) {
            // Generate UUID
            UUID uuid;
            UuidCreate(&uuid);
            wchar_t* uuid_str = nullptr;
            UuidToStringW(&uuid, (RPC_WSTR*)&uuid_str);
            if (!uuid_str) {
                throw std::runtime_error("Failed to generate UUID");
            }
            std::wstring guid = L"{" + std::wstring(uuid_str) + L"}";
            RpcStringFreeW((RPC_WSTR*)&uuid_str);

            fs::path guid_path = plugin_path / guid;

            // Create GUID directory
            fs::create_directory(guid_path, ec);
            if (ec) {
                throw std::runtime_error("Failed to create GUID directory: " + ec.message());
            }

            // Step 6: Create files (dummy content for this example)
            std::wofstream config_file((guid_path / L"nvigi.model.config.json").wstring());
            if (!config_file.is_open()) {
                throw std::runtime_error("Failed to create config file");
            }
            config_file << L"{\"example\": \"config\"}\n"; // Dummy JSON
            config_file.close();

            std::wofstream model_file((guid_path / L"model.gguf").wstring(), std::ios::binary);
            if (!model_file.is_open()) {
                throw std::runtime_error("Failed to create model file");
            }
            model_file << L"Dummy GGUF content"; // Dummy binary content
            model_file.close();
        }
    }

    // Step 7: Return the root path with \\?\ if long_path is true
    return root;
}

TEST_CASE("processDirectory", "[ai][models]")
{
    try {
        PluginConfig config{
            L"llama",    // name
            L"gpu",      // backend
            2,           // num_plugins
            3            // num_guids_per_plugin
        };

        // Test with long path
        std::wstring long_root = create_plugin_temp_directory(true, config);
        std::wcout << L"Long path root: " << long_root << L"\n";
        std::wcout << L"Length: " << long_root.length() << L" characters\n";

        std::wstring pluginDir = L"nvigi.plugin." + config.name + L"." + config.backend;

        // Test processDirectory with long path
        {
            json modelInfo;
            auto utf8PathToModels = (fs::path(long_root) / pluginDir).u8string();
            REQUIRE(nvigi::ai::processDirectory(utf8PathToModels, modelInfo, nullptr, { "gguf" }));
            for (auto& section : modelInfo)
            {
                REQUIRE(section.contains("guid"));
                REQUIRE(section.contains("gguf"));
                std::string file = section["gguf"][0];
                REQUIRE(fs::exists(file));
            }
        }
        
        // Cleanup (optional)
        fs::remove_all(long_root);
        
        // Test with regular path
        std::wstring reg_root = create_plugin_temp_directory(false, config);
        std::wcout << L"Regular root: " << reg_root << L"\n";

        {
            json modelInfo;
            auto utf8PathToModels = (fs::path(reg_root) / pluginDir).u8string();
            REQUIRE(nvigi::ai::processDirectory(utf8PathToModels, modelInfo, nullptr, { "gguf" }));
            for (auto& section : modelInfo)
            {
                REQUIRE(section.contains("guid"));
                REQUIRE(section.contains("gguf"));
                std::string file = section["gguf"][0];
                REQUIRE(fs::exists(file));
            }
        }

        // Cleanup
        fs::remove_all(reg_root);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

#endif // NVIGI_WINDOWS