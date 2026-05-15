// SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <regex>

#include "source/utils/nvigi.ai/nvigi_ai.h"
#include "source/core/nvigi.file/file.h"
#include "source/core/nvigi.api/nvigi_io.h"
#include "external/json/source/nlohmann/json.hpp"
using json = nlohmann::json;

namespace nvigi
{
namespace ai
{

inline bool isHexChar(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

inline bool isGuid(const std::string& str) {
    std::regex guidPattern(R"(\{[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\})");
    return std::regex_match(str, guidPattern);
}

struct ModelCommonInfo
{
    std::string path;
    size_t vram;
};

constexpr const char* kInvalidModelName = "unknown";
constexpr const char* kModelConfigFile = "nvigi.model.config.json";

inline void processGUIDDirectory(const std::string& guid, const std::u8string& _directory, json& modelInfo, const CommonCreationParameters* params, const std::vector<std::string>& extensions, std::map<std::string, std::vector<std::string>>& fileList, bool& useLocalFiles)
{
    std::u8string directory;
    file::getOSValidPath(_directory, directory);
    for (auto const& files : fs::directory_iterator{ directory })
    {
        if (files.is_directory())
        {
            processGUIDDirectory(guid, files.path().u8string(), modelInfo, params, extensions, fileList, useLocalFiles);
        }
        else
        {
            auto fileName = files.path().filename().string();
            if (fileName == kModelConfigFile || fileName == "model.json") // legacy name check
            {
                // Make sure we did not read JSON from another location
                std::string name = modelInfo.contains(guid) ? modelInfo[guid]["name"] : kInvalidModelName;
                if (name == kInvalidModelName)
                {
                    auto jsonText = file::read(files.path().c_str());
                    if (!jsonText.empty())
                    {
                        // safety null in case the JSON string is not null-terminated (found by AppVerif)
                        jsonText.push_back(0);
                        std::istringstream stream((const char*)jsonText.data());
                        json modelCfg;
                        stream >> modelCfg;
                        modelInfo[guid] = modelCfg;
                        
                        // Check if JSON contains local_files array
                        // If so, we'll use those files instead of searching by extension
                        if (modelCfg.contains("model") && modelCfg["model"].contains("local_files"))
                        {
                            useLocalFiles = true;
                            NVIGI_LOG_VERBOSE("Model config for GUID %s contains 'local_files', will use specified files instead of extension search", guid.c_str());
                        }
                    }
                }
                else
                {
                    continue;
                }
            }
            
            // Only search by extension if we're not using local_files from JSON
            if (!useLocalFiles)
            {
                auto ext = files.path().extension().string().substr(1);
                if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
                {
                    // Adding name to the path can go over the MAX_PATH limit on Windows so we need to handle it correctly
                    auto name = files.path().filename().u8string();
                    std::u8string modelPath = (fs::path(directory) / name).u8string();
                    if (file::getOSValidPath(modelPath, modelPath))
                    {
                        fileList[ext].push_back(std::string(modelPath.begin(), modelPath.end()));
                    }
                    else
                    {
                        NVIGI_LOG_WARN("Skipping invalid path '%S' for model GUID {%s}", fs::path(modelPath).wstring().c_str(), guid.c_str());
                    }
                }
            }
        }
    }
}

inline bool processDirectory(const std::u8string& _directory, json& modelInfo, const CommonCreationParameters* params, const std::vector<std::string>& extensions, bool optional = false)
{
    std::u8string directory;
    if(!file::getOSValidPath(_directory, directory))
    {
        if (optional) return true;
        // fs::path constructor does NOT throw exceptions for invalid paths etc. so using it to print correctly unicode paths
        NVIGI_LOG_ERROR("Not a valid directory '%S'", fs::path(_directory).wstring().c_str());
        return false;
    }
    try
    {
        for (auto const& entry : fs::directory_iterator{ directory })
        {
            if (entry.is_directory())
            {
                auto guid = entry.path().filename().string();
                if (isGuid(guid))
                {
                    // NOTE: At this point this could be a long path on Windows so '/' slashes are not allowed
                    // 
                    // std::filesystem::path / operator will use the correct path separator
                    auto guidDirectory = fs::path(directory) / std::u8string(guid.begin(),guid.end());
                    std::map<std::string, std::vector<std::string>> fileList{};
                    bool useLocalFiles = false;
                    
                    // Scan directory and read JSON config
                    processGUIDDirectory(guid, guidDirectory.u8string(), modelInfo, params, extensions, fileList, useLocalFiles);

                    modelInfo[guid]["guid"] = guid;
                    if (!modelInfo[guid].contains("vram"))
                        modelInfo[guid]["vram"] = 0;
                    if (!modelInfo[guid].contains("name"))
                        modelInfo[guid]["name"] = kInvalidModelName;

                    // Check if JSON contains local_files and handle accordingly
                    if (useLocalFiles && modelInfo[guid].contains("model") && modelInfo[guid]["model"].contains("local_files"))
                    {
                        // HYBRID SCENARIO: GUID + path provided, but JSON has local_files
                        // Use the file names from local_files and build full paths
                        NVIGI_LOG_INFO("Using local_files from model config for GUID %s", guid.c_str());
                        
                        try {
                            std::vector<std::string> localFiles = modelInfo[guid]["model"]["local_files"].get<std::vector<std::string>>();
                            
                            for (const auto& fileName : localFiles)
                            {
                                // Build full path: {directory}/{fileName}
                                auto path = fs::path(guidDirectory) / std::u8string(fileName.begin(), fileName.end());
                                if (optional)
                                {
                                    // Find and remove last '/configs/' subdirectory in the path we appended, that is our optional file structure where 'configs' are used just to store model cards (json)
                                    auto configsIndex = path.string().rfind("\\configs\\");
                                    if (configsIndex != std::string::npos)
                                    {
                                        path = path.string().substr(0, configsIndex) + path.string().substr(configsIndex + 8);
                                    }
                                }

                                std::u8string fullPath = path.u8string();
                                std::u8string validPath;
                                
                                if (file::getOSValidPath(fullPath, validPath))
                                {
                                    // Extract extension to categorize the file
                                    fs::path filePath(fileName);
                                    auto ext = filePath.extension().string();
                                    if (!ext.empty() && ext[0] == '.') {
                                        ext = ext.substr(1);  // Remove leading dot
                                    }
                                    
                                    if (!ext.empty())
                                    {
                                        fileList[ext].push_back(std::string(validPath.begin(), validPath.end()));
                                        NVIGI_LOG_VERBOSE("Added local_file: %s (ext: %s)", fileName.c_str(), ext.c_str());
                                    }
                                }
                                else
                                {
                                    NVIGI_LOG_WARN("Invalid path for local_file '%s' in GUID {%s}", fileName.c_str(), guid.c_str());
                                }
                            }
                        }
                        catch (const std::exception& e) {
                            NVIGI_LOG_ERROR("Failed to process local_files for GUID %s: %s", guid.c_str(), e.what());
                        }
                    }
                    
                    // Store file lists in model info
                    // If we did not find a single file for any requested extension then model needs to be downloaded
                    bool requiresDownload = true;
                    for (auto& ext : extensions)
                    {
                        requiresDownload &= fileList[ext].empty();
                        modelInfo[guid][ext] = fileList[ext];
                    }
                    modelInfo[guid]["requiresDownload"] = requiresDownload;
                }
                else
                {
                    // GUID expected
                    return false;
                }
            }
        }
    }
    catch (std::exception& e)
    {
        NVIGI_LOG_ERROR("Exception %s", e.what());
        return false;
    }
    return true;
};

inline bool findFilePath(const json& guidInfo, const char* fileName, std::string& filePath)
{
    //std::string tmp = guidInfo.dump(1, ' ', false, json::error_handler_t::replace);
    //NVIGI_LOG_VERBOSE("%s", tmp.c_str());

    fs::path path(fileName);
    if (!path.has_extension()) return false;
    auto ext = path.extension().string().substr(1);
    if (!guidInfo.contains(ext)) return false;
    auto files = guidInfo[ext];
    if (files.empty()) return false;
    for (auto file : files)
    {
        if (file.operator std::string().contains(fileName))
        {
            filePath = file.operator std::string();
            return true;
        }
    }
    return false;
}

constexpr const char* kSyntheticKey = "callback-model";

//! Unified model discovery function - handles ALL three approaches automatically
//! 
//! This function intelligently detects which approach to use based on input parameters:
//! 
//! APPROACH 3 (Modern - IO Callbacks): ioCallbacks != nullptr
//!    - Requires: modelCardJSON in params
//!    - Parses JSON directly from params->modelCardJSON
//!    - Extracts model.local_files array
//!    - Returns file names as-is (virtual paths)
//!    - No filesystem access, no GUID/path required
//! 
//! APPROACH 2 (Hybrid): ioCallbacks == nullptr, JSON has local_files
//!    - Requires: modelGUID and utf8PathToModels in params
//!    - Scans filesystem for GUID directory
//!    - Reads nvigi.model.config.json from disk
//!    - If JSON contains model.local_files: uses those file names
//!    - Builds full paths: {utf8PathToModels}/{pluginDir}/{GUID}/{fileName}
//! 
//! APPROACH 1 (Legacy): ioCallbacks == nullptr, JSON lacks local_files
//!    - Requires: modelGUID and utf8PathToModels in params
//!    - Scans filesystem for GUID directory
//!    - Reads nvigi.model.config.json from disk
//!    - Scans directory for files matching extensions
//!    - Returns full paths to discovered files
//! 
//! USAGE: Plugin just calls this once and it works for all scenarios!
//! 
//! @param params CommonCreationParameters (see requirements per approach above)
//! @param extensions File extensions to search for (used in Approach 1 only)
//! @param modelInfo Output JSON containing model information and file paths
//! @param ioCallbacks Optional callbacks (nullptr = filesystem, non-null = Approach 3)
//! @param additionalModelInfo Optional output for additional model directory
//! @return true on success, false on error with detailed logging
//! 
inline bool findModels(const CommonCreationParameters* params, const std::vector<std::string>& extensions, json& modelInfo, const FileIOCallbacks* ioCallbacks = nullptr, json* additionalModelInfo = nullptr)
{
    modelInfo.clear();
    
    if (extensions.empty())
    {
        NVIGI_LOG_ERROR("Must provide at least one model extension");
        return false;
    }

    std::string pluginDir = plugin::getContext()->framework->getModelDirectoryForPlugin(plugin::getContext()->info.id).c_str();

    // ========================================================================
    // APPROACH 3: IO Callbacks with Direct JSON Card
    // ========================================================================
    if (ioCallbacks)
    {
        // Validate required parameters for Approach 3
        if (!params->modelCardJSON || strlen(params->modelCardJSON) == 0)
        {
            NVIGI_LOG_ERROR("When using FileIOCallbacks, modelCardJSON must be provided in CommonCreationParameters");
            return false;
        }
        
        // Parse the JSON card provided by the host
        json modelCard;
        try {
            modelCard = json::parse(params->modelCardJSON);
        } catch (const std::exception& e) {
            NVIGI_LOG_ERROR("Failed to parse modelCardJSON: %s", e.what());
            return false;
        }
        
        // Extract local_files array from JSON card
        if (!modelCard.contains("model") || !modelCard["model"].contains("local_files"))
        {
            NVIGI_LOG_ERROR("Model card JSON must contain 'model.local_files' section");
            return false;
        }
        
        std::vector<std::string> files;
        try {
            files = modelCard["model"]["local_files"].get<std::vector<std::string>>();
        } catch (const std::exception& e) {
            NVIGI_LOG_ERROR("Failed to extract local_files from JSON: %s", e.what());
            return false;
        }
        
        if (files.empty())
        {
            NVIGI_LOG_ERROR("No files specified in model.local_files section");
            return false;
        }
        
        NVIGI_LOG_INFO("Found %zu model file(s) in JSON card", files.size());
        
        // Create a synthetic GUID for indexing (use "callback-model" as key)
        modelInfo[kSyntheticKey] = modelCard;
        modelInfo[kSyntheticKey]["guid"] = kSyntheticKey;
        
        // Organize files by extension for consistency with other approaches
        for (const auto& fileName : files)
        {
            fs::path filePath(fileName);
            auto ext = filePath.extension().string();
            if (!ext.empty() && ext[0] == '.') {
                ext = ext.substr(1);  // Remove leading dot
            }
            
            if (!ext.empty())
            {
                if (!modelInfo[ai::kSyntheticKey].contains(ext)) {
                    modelInfo[ai::kSyntheticKey][ext] = json::array();
                }
                modelInfo[ai::kSyntheticKey][ext].push_back(fileName);
                NVIGI_LOG_VERBOSE("Registered file: %s (ext: %s)", fileName.c_str(), ext.c_str());
            }
        }
        
        modelInfo[ai::kSyntheticKey]["requiresDownload"] = false;
        
        return true;
    }
    
    // ========================================================================
    // APPROACH 1 or 2: Filesystem-Based Discovery (with or without local_files)
    // ========================================================================
    
    // Validate required parameters for Approach 1 & 2
    if (!params->utf8PathToModels || strlen(params->utf8PathToModels) == 0)
    {
        NVIGI_LOG_ERROR("When NOT using FileIOCallbacks, utf8PathToModels must be provided in CommonCreationParameters");
        return false;
    }
    
    // It is OK not to have model GUID when enumerating all models available in the filesystem, but if GUID is provided it needs to be valid
    if (params->modelGUID)
    {
        if (!isGuid(params->modelGUID))
        {
            NVIGI_LOG_ERROR("Provided model GUID '%s' is invalid - expecting registry format {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}", params->modelGUID);
            return false;
        }

        // Initialize model info in case we don't find any files
        for (auto& ext : extensions)
        {
            modelInfo[params->modelGUID][ext] = {};
        }
        modelInfo[params->modelGUID]["vram"] = 0;
        modelInfo[params->modelGUID]["name"] = kInvalidModelName;
    }

    // Log what we are looking for and where
    auto tmp = extensions;
    auto extStr = tmp.back();
    tmp.pop_back();
    while (!tmp.empty())
    {
        extStr += "," + tmp.back();
        tmp.pop_back();
    }    
    NVIGI_LOG_INFO("Using GUID-based model discovery via filesystem");
    NVIGI_LOG_INFO("# path: '%S'", fs::path(params->utf8PathToModels).wstring().c_str());
    NVIGI_LOG_INFO("# plugin: '%s'", pluginDir.c_str());
    NVIGI_LOG_INFO("# GUID: %s", params->modelGUID ? params->modelGUID : "any");
    NVIGI_LOG_INFO("# extension(s): [%s]", extStr.c_str());

    // First try optional "configs" path to collect JSON files
    auto directory = std::u8string((const char8_t*)(params->utf8PathToModels + std::string("/configs/") + pluginDir).c_str());
    processDirectory(directory, modelInfo, params, extensions, true);
    
    // Now collect model info and any JSON files not provided under "configs"
    directory = std::u8string((const char8_t*)(params->utf8PathToModels + std::string("/") + pluginDir).c_str());
    if (!processDirectory(directory, modelInfo, params, extensions)) return false;
    
    // Log which sub-approach was detected
    if (params->modelGUID && modelInfo[params->modelGUID].contains("model") &&
        modelInfo[params->modelGUID]["model"].contains("local_files")) {
        NVIGI_LOG_VERBOSE("Model config contains local_files (Approach 2: Hybrid)");
    }
    else {
        NVIGI_LOG_VERBOSE("Using extension-based discovery (Approach 1: Legacy)");
    }

    if (additionalModelInfo)
    {
        if (!params->utf8PathToAdditionalModels)
        {
            NVIGI_LOG_ERROR("Path to additional models must be provided when requesting additional model information");
            return false;
        }
        directory = std::u8string((const char8_t*)(params->utf8PathToAdditionalModels + std::string("/") + pluginDir + std::string("/")).c_str());
        if (!processDirectory(directory, *additionalModelInfo, params, extensions)) return false;
    }

    return true;
}


//! Interface 'CommonCapsData'
//!
//! {E8C395AB-CF24-4CF3-9A7A-D3F8AAB57AAF}
struct alignas(8) CommonCapsData
{
    CommonCapsData() { };
    NVIGI_UID(UID({ 0xe8c395ab, 0xcf24, 0x4cf3,{0x9a, 0x7a, 0xd3, 0xf8, 0xaa, 0xb5, 0x7a, 0xaf} }), kStructVersion1)

    types::vector<const char*> names;
    types::vector<const char*> guids;
    types::vector<size_t> vrams;
    types::vector<uint32_t> flags;

    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVIGI_VALIDATE_STRUCT(CommonCapsData)

inline void freeCommonCapsAndRequirements(CommonCapsData& data)
{
    for (auto i : data.names)
    {
        delete[] i;
    }
    for (auto i : data.guids)
    {
        delete[] i;
    }
    data.names.clear();
    data.guids.clear();
    data.vrams.clear();
    data.flags.clear();
}

inline bool populateCommonCapsAndRequirements(CommonCapsData& data, const CommonCreationParameters& params, CommonCapabilitiesAndRequirements& caps, const json& modelInfo)
{
    freeCommonCapsAndRequirements(data);

    //std::string tmp = modelInfo.dump(1, ' ', false, json::error_handler_t::replace);
    //NVIGI_LOG_VERBOSE("%s", tmp.c_str());

    auto allocStr = [](const std::string& s)->const char*
    {
        char* p = new char[s.size() + 1];
        strcpy_s(p, s.size() + 1, s.data());
        return p;
    };
    for (auto& model : modelInfo) try
    {
        size_t vram = 0;
        vram = extra::getJSONValue(model, "vram", vram); // allow missing vram info
        std::string guid = model["guid"];
        // If model is provided we filter by its GUID
        if ((!params.modelGUID || params.modelGUID == guid) &&
            // If cloud backend then VRAM plays no role so 
            (caps.supportedBackends == nvigi::InferenceBackendLocations::eCloud || vram <= params.vramBudgetMB))
        {
            data.vrams.push_back(vram);
            data.guids.push_back(allocStr(guid));
            std::string s = model["name"];
            data.names.push_back(allocStr(s));
            bool requiresDownload = model["requiresDownload"];
            data.flags.push_back(requiresDownload ? kModelFlagRequiresDownload : 0);
        }
    }
    catch (std::exception&)
    {
        return false;
    }

    caps.numSupportedModels = data.guids.size();
    caps.modelMemoryBudgetMB = data.vrams.data();
    caps.supportedModelGUIDs = data.guids.data();
    caps.supportedModelNames = data.names.data();
    caps.modelFlags = data.flags.data();

    return true;
}

//! A2X HELPER
//! 
struct A2XAudioProcessor
{
    A2XAudioProcessor(float _dt, int _sampleRate, int _bufferLength, int _bufferOffset, const std::vector<float>& _trackData) :
        sampleRate(_sampleRate), bufferLength(_bufferLength), bufferOffset(_bufferOffset), dt(_dt), trackData(_trackData)
    {
        numFrames = size_t(std::ceil((float(trackData.size()) / float(sampleRate)) / dt));
    }

    void reset() { t = 0; frameIndex = 0; }

    size_t getChunkSize() const
    {
        return size_t(std::ceil(trackData.size() / numFrames));
    }

    bool getNextAudioChunk(std::vector<float>& audioBuffer, bool* lastChunk = nullptr, size_t* index = nullptr)
    {
        if (frameIndex >= numFrames) return false;

        int sample = int(std::round(t * float(sampleRate)));
        int offset = sample - bufferOffset;
        int begin = std::max(0, -offset);
        int end = std::min(static_cast<int>(bufferLength), static_cast<int>(trackData.size()) - offset);

        audioBuffer.resize(bufferLength, 0.0f);
        memset(audioBuffer.data(), 0, audioBuffer.size() * sizeof(float));
        int copyLength = std::min(end - begin, bufferLength - begin); // Calculate how many elements we can actually copy
        for (int idx = 0; idx < copyLength; idx++)
        {
            audioBuffer[begin + idx] = trackData[offset + begin + idx];
        }
        if(index) *index = frameIndex;
        t += dt;
        frameIndex++;
        if(lastChunk) *lastChunk = frameIndex == numFrames;
        return true;
    }

    int sampleRate;
    int bufferLength;
    int bufferOffset;
    float t = 0;
    float dt = 0;
    size_t numFrames;
    size_t frameIndex = 0;
    std::vector<float> trackData;
};

constexpr const char* kPromptTemplate = "prompt_template";
constexpr const char* kTurnTemplate = "turn_template";

//! PROMPT HELPER
//! 
inline std::string generatePrompt(const json& model, const std::string& system, const std::string& user, const std::string& assistant)
{
    std::string prompt;
    if (model.contains(kPromptTemplate))
    {
        std::vector<std::string> tmpl = model[kPromptTemplate];
        for (auto& s : tmpl)
        {
            if (s == "$system") prompt += system;
            else if (s == "$user") prompt += user;
            else if (s == "$assistant") prompt += assistant;
            else prompt += s;
        }
    }
    else
    {
        // Default - if no prompt template, user must put it all in the user prompt.
        prompt += user;
    }
    return prompt;
}

//! TURN HELPER (CHAT/INTERACTIVE MODE)
//! 
inline std::string generateTurn(const json& model, const std::string& user, const std::string& assistant)
{
    std::string prompt;
    if (model.contains(kTurnTemplate))
    {
        std::vector<std::string> tmpl = model[kTurnTemplate];
        for (auto& s : tmpl)
        {
            if (s == "$user") prompt += user;
            else if (s == "$assistant") prompt += assistant;
            else prompt += s;
        }
    }
    else
    {
        // Default
        prompt += "\nInstruct:" + user + "\nOutput:" + assistant;
    }
    return prompt;
}

}
}