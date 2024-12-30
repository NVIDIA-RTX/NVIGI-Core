// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <regex>

#include "source/utils/nvigi.ai/nvigi_ai.h"
#include "source/core/nvigi.file/file.h"
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

inline void processGUIDDirectory(const std::string& guid, const std::u8string& _directory, json& modelInfo, const CommonCreationParameters* params, const std::vector<std::string>& extensions, std::map<std::string, std::vector<std::string>>& fileList)
{
    std::u8string directory;
    file::getOSValidPath(_directory, directory);
    for (auto const& files : fs::directory_iterator{ directory })
    {
        if (files.is_directory())
        {
            processGUIDDirectory(guid, files.path().u8string(), modelInfo, params, extensions, fileList);
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
                    }
                }
                else
                {
                    continue;
                }
            }
            auto ext = files.path().extension().string().substr(1);
            if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
            {
                auto name = files.path().filename().u8string();
                std::u8string modelPath;
                if (!file::getOSValidPath(directory + u8"/" + name, modelPath))
                {
                    // Should never happen really
                    return;
                }
                fileList[ext].push_back(std::string(modelPath.begin(), modelPath.end()));
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
                    auto guidDirectory = directory + u8"/" + std::u8string(guid.begin(),guid.end());
                    std::map<std::string, std::vector<std::string>> fileList{};
                    processGUIDDirectory(guid,guidDirectory, modelInfo, params, extensions, fileList);

                    modelInfo[guid]["guid"] = guid;
                    if (!modelInfo[guid].contains("vram"))
                        modelInfo[guid]["vram"] = 0;
                    if (!modelInfo[guid].contains("name"))
                        modelInfo[guid]["name"] = kInvalidModelName;

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

//! Finds model info for the given plugin and model location
//! 
//! Collects model names, GUIDs, VRAM usage, utf-8 encoded paths to 
//! files based on the provided extension(s) and any other information 
//! stored in the `model.json` file.
//! 
//! Model locations are provided via CommonCreationParameters::utf8PathToModels and the optional utf8PathToAdditonalModels
//! 
//! All information is returned in a JSON format
//! 
inline bool findModels(const CommonCreationParameters* params, const std::vector<std::string>& extensions, json& modelInfo, json* additionalModelInfo = nullptr)
{
    modelInfo.clear();
    if (params->modelGUID)
    {
        if (!isGuid(params->modelGUID))
        {
            NVIGI_LOG_ERROR("Provided model GUID '%s' is invalid - expecting registry format {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}", params->modelGUID);
            return false;
        }
        // In case we don't find any files
        for (auto& ext : extensions)
        {
            modelInfo[params->modelGUID][ext] = {};
        }
        modelInfo[params->modelGUID]["vram"] = 0;
        modelInfo[params->modelGUID]["name"] = kInvalidModelName;
    }

    if (extensions.empty())
    {
        NVIGI_LOG_ERROR("Must provide at least one model extension");
        return false;
    }

    std::string pluginDir = plugin::getContext()->framework->getModelDirectoryForPlugin(plugin::getContext()->info.id).c_str();

    // Log what we are looking for and where to make it easier to debug any issues with missing models, mixed up extensions etc.
    auto tmp = extensions;
    auto extStr = tmp.back();
    tmp.pop_back();
    while (!tmp.empty())
    {
        extStr = "," + tmp.back();
        tmp.pop_back();
    }    
    NVIGI_LOG_INFO("Looking for model(s) in '%S':", fs::path(params->utf8PathToModels).wstring().c_str());
    NVIGI_LOG_INFO("# plugin: '%s'", pluginDir.c_str());
    NVIGI_LOG_INFO("# GUID: %s", params->modelGUID ? params->modelGUID : "any");
    NVIGI_LOG_INFO("# extension(s): [%s]", extStr.c_str());

    // First try optional "configs" path to collect JSON files
    auto directory = std::u8string((const char8_t*)(params->utf8PathToModels + std::string("/configs/") + pluginDir).c_str());
    processDirectory(directory, modelInfo, params, extensions, true);
    // Now collect model info and any JSON files not provided under "configs"
    directory = std::u8string((const char8_t*)(params->utf8PathToModels + std::string("/") + pluginDir).c_str());
    if (!processDirectory(directory, modelInfo, params, extensions)) return false;
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

    //std::string tmp = modelInfo.dump(1, ' ', false, json::error_handler_t::replace);
    //NVIGI_LOG_VERBOSE("%s", tmp.c_str());

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