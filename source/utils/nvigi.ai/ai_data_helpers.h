// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <type_traits>
#include <string>
#include <vector>

namespace nvigi::ai
{
//! INFERENCE DATA SLOT HELPERS
//! 
//! Use these classes for convenient creation of various data slots using STL
//! 

bool updateInferenceDataText(InferenceDataText* slot, const std::string& text)
{
    if (!slot) return false;
    auto cpuBuffer = castTo<CpuData>(slot->utf8Text);
    if (!cpuBuffer || !cpuBuffer->buffer || cpuBuffer->sizeInBytes < text.size())
    {
        return false;
    }
    strcpy_s((char*)cpuBuffer->buffer, cpuBuffer->sizeInBytes, text.c_str());
    return true;
}

struct InferenceDataTextHelper
{
    InferenceDataTextHelper() {};

    InferenceDataTextHelper(const char* txt)
    {
        _text = txt;
    }
    InferenceDataTextHelper(const std::string& txt)
    {
        _text = txt;
    }
    operator InferenceDataText* ()
    {
        _data.buffer = _text.data();
        _data.sizeInBytes = _text.length();
        _slot.utf8Text = _data;
        return &_slot;
    };

    InferenceDataText _slot{};
    std::string _text{};
    CpuData _data{};
};

struct InferenceDataByteArrayHelper
{
    InferenceDataByteArrayHelper(const uint8_t* data, size_t size)
    {
        _bytes = std::vector(data, data + size);
    }
    InferenceDataByteArrayHelper(const std::vector<uint8_t>& data)
    {
        _bytes = data;
    }
    operator InferenceDataByteArray* ()
    {
        _data.buffer = _bytes.data();
        _data.sizeInBytes = _bytes.size();
        _slot.bytes = _data;
        return &_slot;
    };

    InferenceDataByteArray _slot{};
    std::vector<uint8_t> _bytes{};
    CpuData _data{};
};

struct InferenceDataAudioHelper
{
    InferenceDataAudioHelper(const InferenceDataAudio* in) { _input = *in; };

    InferenceDataAudioHelper(const int8_t* data, size_t size)
    {
        _samples = std::vector(data, data + size);
    }
    InferenceDataAudioHelper(const std::vector<int8_t>& data)
    {
        _samples = data;
    }
    operator InferenceDataAudio* ()
    {
        _data.buffer = _samples.data();
        _data.sizeInBytes = _samples.size();
        _slot.audio = _data;
        return &_slot;
    };

    bool hasValidInputBuffer() const
    {
        auto cpuBuffer = castTo<CpuData>(_input.audio);
        return cpuBuffer != nullptr;
    }

    bool hasValidInputFormat() const
    {
        return _input.channels == 1 && _input.samplingRate == 16000;
    }

    InferenceDataAudio* getPCM8()
    {
        auto cpuBuffer = castTo<CpuData>(_input.audio);
        if (!cpuBuffer || !hasValidInputBuffer()) return nullptr;

        if (_input.bitsPerSample == 16)
        {
            std::vector<uint8_t> output;
            output.resize(cpuBuffer->sizeInBytes / sizeof(int16_t));
            std::transform((int16_t*)cpuBuffer->buffer, (int16_t*)cpuBuffer->buffer + output.size(), output.begin(), [](int16_t i)
                {
                    return (uint8_t)(std::numeric_limits<uint8_t>::max() * 0.5f * (float(i) / (float)std::numeric_limits<int16_t>::max() + 1.0f));
                });
            _samples.resize(output.size());
            memcpy_s(_samples.data(), _samples.size(), output.data(), _samples.size());
        }
        else if (_input.bitsPerSample == 8)
        {
            _samples.resize(cpuBuffer->sizeInBytes);
            memcpy_s(_samples.data(), cpuBuffer->sizeInBytes, cpuBuffer->buffer, cpuBuffer->sizeInBytes);
        }
        else if (_input.bitsPerSample == 32)
        {
            std::vector<uint8_t> output;
            output.resize(cpuBuffer->sizeInBytes / sizeof(int32_t));
            std::transform((int32_t*)cpuBuffer->buffer, (int32_t*)cpuBuffer->buffer + output.size(), output.begin(), [](int32_t i)
                {
                    return (uint8_t)(std::numeric_limits<uint8_t>::max() * 0.5f * (float(i) / 2147483648.0f + 1.0f));
                });
            _samples.resize(output.size());
            memcpy_s(_samples.data(), _samples.size(), output.data(), _samples.size());
        }
        else
        {
            return nullptr;
        }
        _data.buffer = _samples.data();
        _data.sizeInBytes = _samples.size();
        _slot.audio = _data;
        _slot.channels = _input.channels;
        _slot.bitsPerSample = 8;
        _slot.samplingRate = _input.samplingRate;
        return &_slot;
    };

    InferenceDataAudio* getPCM16()
    {
        auto cpuBuffer = castTo<CpuData>(_input.audio);
        if (!cpuBuffer || !hasValidInputBuffer()) return nullptr;

        if (_input.bitsPerSample == 16)
        {
            assert(cpuBuffer->sizeInBytes % 2 == 0);
            _samples.resize(cpuBuffer->sizeInBytes);
            memcpy_s(_samples.data(), cpuBuffer->sizeInBytes, cpuBuffer->buffer, cpuBuffer->sizeInBytes);
        }
        else if (_input.bitsPerSample == 8)
        {
            std::vector<int16_t> output;
            output.resize(cpuBuffer->sizeInBytes);
            std::transform((uint8_t*)cpuBuffer->buffer, (uint8_t*)cpuBuffer->buffer + cpuBuffer->sizeInBytes, output.begin(), [](uint8_t i)
                {
                    return (int16_t)(std::numeric_limits<int16_t>::max() * (i / 128.0f - 1.0f));
                });
            _samples.resize(output.size() * 2);
            memcpy_s(_samples.data(), _samples.size(), output.data(), _samples.size());
        }
        else if (_input.bitsPerSample == 32)
        {
            std::vector<int16_t> output;
            output.resize(cpuBuffer->sizeInBytes / sizeof(int32_t));
            std::transform((int32_t*)cpuBuffer->buffer, (int32_t*)cpuBuffer->buffer + output.size(), output.begin(), [](int32_t i)
                {
                    return (int16_t)(std::numeric_limits<int16_t>::max() * (i / 2147483648.0f));
                });
            _samples.resize(output.size() * 2);
            memcpy_s(_samples.data(), _samples.size(), output.data(), _samples.size());
        }
        else
        {
            return nullptr;
        }
        _data.buffer = _samples.data();
        _data.sizeInBytes = _samples.size();
        _slot.audio = _data;
        _slot.channels = _input.channels;
        _slot.bitsPerSample = 16;
        _slot.samplingRate = _input.samplingRate;
        return &_slot;
    };

    InferenceDataAudio* getPCM32()
    {
        auto cpuBuffer = castTo<CpuData>(_input.audio);
        if (!cpuBuffer || !hasValidInputBuffer()) return nullptr;

        if (_input.bitsPerSample == 16)
        {
            std::vector<int32_t> output;
            output.resize(cpuBuffer->sizeInBytes / sizeof(int16_t));
            std::transform((int16_t*)cpuBuffer->buffer, (int16_t*)cpuBuffer->buffer + output.size(), output.begin(), [](int16_t i)
                {
                    return int32_t((float(i) / 32768.0f) * 2147483648.0f);
                });
            _samples.resize(output.size() * sizeof(float));
            memcpy_s(_samples.data(), _samples.size(), output.data(), _samples.size());
        }
        else if (_input.bitsPerSample == 8)
        {
            std::vector<int32_t> output;
            output.resize(cpuBuffer->sizeInBytes);
            std::transform((uint8_t*)cpuBuffer->buffer, (uint8_t*)cpuBuffer->buffer + cpuBuffer->sizeInBytes, output.begin(), [](uint8_t i)
                {
                    return int32_t((float(i) / 128.0f - 1.0f) * 2147483648.0f);
                });
            _samples.resize(output.size() * sizeof(int32_t));
            memcpy_s(_samples.data(), _samples.size(), output.data(), _samples.size());
        }
        else if (_input.bitsPerSample == 32)
        {
            assert(cpuBuffer->sizeInBytes % 4 == 0);
            _samples.resize(cpuBuffer->sizeInBytes);
            memcpy_s(_samples.data(), cpuBuffer->sizeInBytes, cpuBuffer->buffer, cpuBuffer->sizeInBytes);
        }
        else
        {
            return nullptr;
        }
        _data.buffer = _samples.data();
        _data.sizeInBytes = _samples.size();
        _slot.audio = _data;
        _slot.channels = _input.channels;
        _slot.bitsPerSample = 32;
        _slot.samplingRate = _input.samplingRate;
        return &_slot;
    };

    bool getFloat(std::vector<float>& output)
    {
        auto cpuBuffer = castTo<CpuData>(_input.audio);
        if (!cpuBuffer || !hasValidInputBuffer()) return false;

        if (_input.dataType == AudioDataType::eRawFP32)
        {
            output.resize(cpuBuffer->sizeInBytes / sizeof(float));
            memcpy_s(output.data(), cpuBuffer->sizeInBytes, cpuBuffer->buffer, cpuBuffer->sizeInBytes);
        }
        else if (_input.bitsPerSample == 16)
        {
            output.resize(cpuBuffer->sizeInBytes / sizeof(int16_t));
            std::transform((int16_t*)cpuBuffer->buffer, (int16_t*)cpuBuffer->buffer + output.size(), output.begin(), [](int16_t i)
                {
                    return (float(i) / 32768.0f);
                });
        }
        else if (_input.bitsPerSample == 8)
        {
            output.resize(cpuBuffer->sizeInBytes);
            std::transform((uint8_t*)cpuBuffer->buffer, (uint8_t*)cpuBuffer->buffer + cpuBuffer->sizeInBytes, output.begin(), [](uint8_t i)
                {
                    return (float(i) / 128.0f - 1.0f);
                });
        }
        else if (_input.bitsPerSample == 32)
        {
            output.resize(cpuBuffer->sizeInBytes / sizeof(float));
            std::transform((int32_t*)cpuBuffer->buffer, (int32_t*)cpuBuffer->buffer + output.size(), output.begin(), [](int32_t i)
                {
                    return float(i) / 2147483648.0f;
                });
        }
        else
        {
            return false;
        }
        return true;
    };

    InferenceDataAudio* getPCM16FromFloat(const std::vector<float>& audioTrack)
    {
        std::vector<int16_t> output(audioTrack.size());
        std::transform(audioTrack.data(), audioTrack.data() + audioTrack.size(), output.begin(), [](float i)
            {
                return int16_t(i * std::numeric_limits<int16_t>::max());
            });
        _samples.resize(output.size() * sizeof(int16_t));
        memcpy_s(_samples.data(), _samples.size(), output.data(), _samples.size());
        _data.buffer = _samples.data();
        _data.sizeInBytes = _samples.size();
        _slot.audio = _data;
        _slot.channels = 1;
        _slot.bitsPerSample = 16;
        _slot.samplingRate = 16000;
        return &_slot;
    };

    InferenceDataAudio _input{};
    InferenceDataAudio _slot{};
    std::vector<int8_t> _samples{};
    CpuData _data{};
};

}