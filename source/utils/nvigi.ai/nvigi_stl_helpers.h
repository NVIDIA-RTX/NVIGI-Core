// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

// IMPORTANT: This header is intended to be used in the host application only, not in any plugin.
//
// NVIGI does not and must not depend on the C++ standard library in any public structures consumed by plugins,
// hence providing various helper classes to make it easier to work with native NVIGI structures.
//

#include <vector>
#include <string>

#include "nvigi_ai.h"

namespace nvigi
{

// Example usage:
// 
// InferenceDataTextSTLHelper userPrompt("Hello, World!");
// std::vector<nvigi::InferenceDataSlot> slots = { {nvigi::kGPTDataSlotUser, userPrompt} };

struct InferenceDataTextSTLHelper
{
    InferenceDataTextSTLHelper() {};

    InferenceDataTextSTLHelper(const char* txt)
    {
        operator=(txt);
    }
    
    InferenceDataTextSTLHelper(const std::string& txt)
    {
        operator=(txt);
    }

    operator InferenceDataText* ()
    {
        _data.buffer = _text.data();
        _data.sizeInBytes = _text.length();
        _slot.utf8Text = _data;
        return &_slot;
    };

    operator NVIGIParameter* ()
    {
        return *(operator InferenceDataText * ());
    };

    InferenceDataTextSTLHelper& operator=(const std::string& txt)
    {
        _text = txt;
        _data.buffer = _text.data();
        _data.sizeInBytes = _text.length();
        _slot.utf8Text = _data;
        return *this;
    }

    InferenceDataTextSTLHelper& operator=(const char* txt)
    {
        return operator=(std::string(txt));
    }

    InferenceDataTextSTLHelper& operator=(const InferenceDataTextSTLHelper& other)
    {
        _text = other._text;
        _data.buffer = _text.data();
        _data.sizeInBytes = _text.length();
        _slot.utf8Text = _data;
        return *this;
    }

private:
    InferenceDataText _slot{};
    std::string _text{};
    CpuData _data{};
};

// Example usage:
//
// std::vector<uint8_t> data; // This is a placeholder for binary data, will be filled by the user
// nvigi::InferenceDataByteArraySTLHelper binaryData(data);

struct InferenceDataByteArraySTLHelper
{
    InferenceDataByteArraySTLHelper() {};

    InferenceDataByteArraySTLHelper(const uint8_t* data, size_t size)
    {
        operator=(std::vector(data, data + size));
    }
    InferenceDataByteArraySTLHelper(const std::vector<uint8_t>& data)
    {
        operator=(data);
    }
    operator InferenceDataByteArray* ()
    {
        _data.buffer = _bytes.data();
        _data.sizeInBytes = _bytes.size();
        _slot.bytes = _data;
        return &_slot;
    };
    operator NVIGIParameter* ()
    {
        return *(operator InferenceDataByteArray * ());
    };
    InferenceDataByteArraySTLHelper& operator=(const std::vector<uint8_t>& data)
    {
        _bytes = data;
        _data.buffer = _bytes.data();
        _data.sizeInBytes = _bytes.size();
        _slot.bytes = _data;
        return *this;
    }
    InferenceDataByteArraySTLHelper& operator=(const InferenceDataByteArraySTLHelper& other)
    {
        _bytes = other._bytes;
        _data.buffer = _bytes.data();
        _data.sizeInBytes = _bytes.size();
        _slot.bytes = _data;
        return *this;
    }
private:
    InferenceDataByteArray _slot{};
    std::vector<uint8_t> _bytes{};
    CpuData _data{};
};

// Example usage:
//
// nvigi::InferenceDataAudioSTLHelper audioData;
// 
// nvigi::utils::RecordingInfo* ri = nvigi::utils::startRecordingAudio();
// ....
// nvigi::utils::stopRecordingAudio(ri, &audioData);
// 
// std::vector<nvigi::InferenceDataSlot> slots = { {nvigi::kASRWhisperDataSlotAudio, audioData} };
// nvigi::InferenceDataSlotArray inputs{ slots.size(), slots.data() };

struct InferenceDataAudioSTLHelper
{   
    InferenceDataAudioSTLHelper() {};

    InferenceDataAudioSTLHelper(const std::vector<int8_t>& data, int channels = 1)
    {
        _samples = data;
        _slot.bitsPerSample = 8;
        _slot.channels = channels;
        _slot.dataType = AudioDataType::ePCM;
    }
    InferenceDataAudioSTLHelper(const std::vector<int16_t>& data, int channels = 1)
    {
        _samples.resize(data.size() * sizeof(int16_t));
        memcpy(_samples.data(), data.data(), _samples.size());
        _slot.bitsPerSample = 16;
        _slot.channels = channels;
        _slot.dataType = AudioDataType::ePCM;
    }
    InferenceDataAudioSTLHelper(const std::vector<int32_t>& data, int channels = 1)
    {
        _samples.resize(data.size() * sizeof(int32_t));
        memcpy(_samples.data(), data.data(), _samples.size());
        _slot.bitsPerSample = 32;
        _slot.channels = channels;
        _slot.dataType = AudioDataType::ePCM;
    }
    InferenceDataAudioSTLHelper(const std::vector<float>& data, int channels = 1)
    {
        _samples.resize(data.size() * sizeof(float));
        memcpy(_samples.data(), data.data(), _samples.size());
        _slot.bitsPerSample = 32;
        _slot.channels = channels;
        _slot.dataType = AudioDataType::eRawFP32;
    }

    operator InferenceDataAudio* ()
    {
        _data.buffer = _samples.data();
        _data.sizeInBytes = _samples.size();
        _slot.audio = _data;
        return &_slot;
    };

    operator NVIGIParameter* ()
    {
        return *(operator InferenceDataAudio * ());
    };

    InferenceDataAudioSTLHelper& operator=(const InferenceDataAudioSTLHelper& other)
    {
        _samples = other._samples;
        _data.buffer = _samples.data();
        _data.sizeInBytes = _samples.size();
        _slot.audio = _data;
        return *this;
    }

    InferenceDataAudioSTLHelper& operator=(const std::vector<int8_t>& data)
    {
        _samples = data;
        _slot.bitsPerSample = 8;
        _slot.channels = 1; // assuming mono
        _slot.dataType = AudioDataType::ePCM;
        return *this;
    }

    InferenceDataAudioSTLHelper& operator=(const std::vector<int16_t>& data)
    {
        _samples.resize(data.size() * sizeof(int16_t));
        memcpy(_samples.data(), data.data(), _samples.size());
        _slot.bitsPerSample = 16;
        _slot.channels = 1; // assuming mono
        _slot.dataType = AudioDataType::ePCM;
        return *this;
    }

    InferenceDataAudioSTLHelper& operator=(const std::vector<int32_t>& data)
    {
        _samples.resize(data.size() * sizeof(int32_t));
        memcpy(_samples.data(), data.data(), _samples.size());
        _slot.bitsPerSample = 32;
        _slot.channels = 1; // assuming mono
        _slot.dataType = AudioDataType::ePCM;
        return *this;
    }

    InferenceDataAudioSTLHelper& operator=(const std::vector<float>& data)
    {
        _samples.resize(data.size() * sizeof(float));
        memcpy(_samples.data(), data.data(), _samples.size());
        _slot.bitsPerSample = 32;
        _slot.channels = 1; // assuming mono
        _slot.dataType = AudioDataType::eRawFP32;
        return *this;
    }

private:
    InferenceDataAudio _slot{};
    std::vector<int8_t> _samples{};
    CpuData _data{};
};

} // namespace nvigi
