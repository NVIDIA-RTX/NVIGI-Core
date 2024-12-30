// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <cstdint>
#include <vector>

using std::fstream;
using std::string;

namespace nvigi::wav
{

struct WAVHeaderInfo {
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    uint32_t dataSize; // Size of the "DATA" chunk
};

bool extractWAVHeader(std::ifstream& file, WAVHeaderInfo& headerInfo) {
    char chunkId[4];
    uint32_t chunkSize;
    char format[4];

    // Read RIFF header
    file.read(chunkId, 4);
    file.read(reinterpret_cast<char*>(&chunkSize), 4);
    file.read(format, 4);
    if (std::strncmp(chunkId, "RIFF", 4) != 0 || std::strncmp(format, "WAVE", 4) != 0) 
    {
        NVIGI_LOG_ERROR("Not a valid WAV file.");
        return false;
    }

    bool fmtChunkFound = false;
    bool dataChunkFound = false;

    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSize), 4);

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            fmtChunkFound = true;
            uint16_t audioFormat, numChannels;
            uint32_t sampleRate, byteRate;
            uint16_t blockAlign, bitsPerSample;

            file.read(reinterpret_cast<char*>(&audioFormat), sizeof(audioFormat));
            file.read(reinterpret_cast<char*>(&numChannels), sizeof(numChannels));
            file.read(reinterpret_cast<char*>(&sampleRate), sizeof(sampleRate));
            file.read(reinterpret_cast<char*>(&byteRate), sizeof(byteRate));
            file.read(reinterpret_cast<char*>(&blockAlign), sizeof(blockAlign));
            file.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(bitsPerSample));

            // Skip the rest of the chunk if it's larger than the expected size for PCM
            if (chunkSize > 16) {
                file.seekg(chunkSize - 16, std::ios::cur);
            }

            headerInfo = { audioFormat, numChannels, sampleRate, byteRate, blockAlign, bitsPerSample, 0 };
        }
        else if (std::strncmp(chunkId, "data", 4) == 0) {
            dataChunkFound = true;
            headerInfo.dataSize = chunkSize;
            // Correct position for reading data, so return true
            return true;
        }
        else {
            // Skip over chunks that are not "fmt " or "data"
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    // If the loop exits without finding both "fmt " and "data", the file is incomplete or corrupt
    if (!fmtChunkFound || !dataChunkFound) {
        NVIGI_LOG_ERROR("Incomplete or corrupt WAV file: missing 'fmt ' or 'data' chunk.");
        return false;
    }

    return true; // This line should not be reached logically, included for completeness
}


bool readAudioFileAs16bit(string input_filename, std::vector<int16_t>& out_audio, WAVHeaderInfo& header)
{
    std::ifstream file(input_filename, std::ios::binary);

    if (!file.is_open()) {
        NVIGI_LOG_ERROR("Failed to open file.");
        return 1;
    }

    if (extractWAVHeader(file, header)) {
        NVIGI_LOG_INFO("Successfully extracted WAV header info.");
        // Additional processing...
    }
    else {
        NVIGI_LOG_ERROR("Failed to extract WAV header info.");
    }

    // Print some information about the WAV file
    NVIGI_LOG_INFO("Channels: %u", header.numChannels);
    NVIGI_LOG_INFO("Sample Rate: %uHz", header.sampleRate);
    NVIGI_LOG_INFO("Bit Depth: %u bits", header.bitsPerSample);
    NVIGI_LOG_INFO("Duration: %.2f seconds", static_cast<float>(header.dataSize) / header.byteRate);

    // Read and process the audio data
    //const std::size_t bufferSize = 1024;
    //char buffer[bufferSize];


    //while (file.tellg() < fileSize) {
    //    file.read(buffer, bufferSize);

    //    // Process the buffer here (e.g., analyze or manipulate the audio data)
    //    // ...

    //    // For this example, we are just reading and printing the bytes
    //    for (std::streamsize i = 0; i < file.gcount(); ++i) {
    //        std::cout << static_cast<int>(static_cast<uint8_t>(buffer[i])) << " ";
    //    }
    //}

    // Assuming little-endian representation for all cases
    if (header.bitsPerSample == 8)          // uint8
    {
        return false;
    }
    else if (header.bitsPerSample == 16)    // int16
    {
        out_audio.resize(header.dataSize / 2); // every two bytes of file data represents one value
        file.read((char*)out_audio.data(), out_audio.size() * sizeof(int16_t));
    }
    else if (header.bitsPerSample == 32)   // int32
    {
        return false;
    }

    file.close();

    return true;
}

bool GetAudioFile(string input_filename, float*& out_audio, unsigned int& out_audio_len)
{
    std::ifstream file(input_filename, std::ios::binary);
    WAVHeaderInfo header;

    if (!file.is_open()) {
        NVIGI_LOG_ERROR("Failed to open file.");
        return 1;
    }

    if (extractWAVHeader(file, header)) {
        NVIGI_LOG_INFO("Successfully extracted WAV header info.");
        // Additional processing...
    }
    else {
        NVIGI_LOG_ERROR("Failed to extract WAV header info.");
    }

    // Print some information about the WAV file
    //std::cout << "Channels: " << header.numChannels << std::endl;
    //std::cout << "Sample Rate: " << header.sampleRate << " Hz" << std::endl;
    //std::cout << "Bit Depth: " << header.bitsPerSample << " bits" << std::endl;
    //std::cout << "Duration: " << static_cast<float>(header.subchunk2Size) / header.byteRate << " seconds" << std::endl;

    // Read and process the audio data
    //const std::size_t bufferSize = 1024;
    //char buffer[bufferSize];


    //while (file.tellg() < fileSize) {
    //    file.read(buffer, bufferSize);

    //    // Process the buffer here (e.g., analyze or manipulate the audio data)
    //    // ...

    //    // For this example, we are just reading and printing the bytes
    //    for (std::streamsize i = 0; i < file.gcount(); ++i) {
    //        std::cout << static_cast<int>(static_cast<uint8_t>(buffer[i])) << " ";
    //    }
    //}
    auto fileSize = header.dataSize;

    // Assuming little-endian representation for all cases
    if (header.bitsPerSample == 8)          // uint8
    {
        out_audio_len = fileSize;        // every byte of file data represents one value
        uint8_t* dyn_buffer = (uint8_t*)malloc(fileSize);
        file.read((char*)dyn_buffer, fileSize);
        out_audio = (float*)malloc(sizeof(float) * out_audio_len);
        for (uint32_t i = 0; i < out_audio_len; ++i)
        {
            out_audio[i] = (uint8_t)dyn_buffer[i] / 128.0f - 1.0f;
        }
        free(dyn_buffer);
    }
    else if (header.bitsPerSample == 16)    // int16
    {
        out_audio_len = fileSize / 2;    // every two bytes of file data represents one value
        int16_t* dyn_buffer = (int16_t*)malloc(fileSize);
        file.read((char*)dyn_buffer, fileSize);
        out_audio = (float*)malloc(sizeof(float) * out_audio_len);
        for (uint32_t i = 0; i < out_audio_len; ++i)
        {
            out_audio[i] = (int16_t)dyn_buffer[i] / 32768.0f;
        }
        free(dyn_buffer);
    }
    else if (header.bitsPerSample == 32)   // int32
    {
        out_audio_len = fileSize / 4;     // every four bytes of file data represents one value
        int32_t* dyn_buffer = (int32_t*)malloc(fileSize);
        file.read((char*)dyn_buffer, fileSize);
        out_audio = (float*)malloc(sizeof(float) * out_audio_len);
        for (uint32_t i = 0; i < out_audio_len; ++i)
        {
            out_audio[i] = (int32_t)dyn_buffer[i] / 2147483648.0f;
        }
        free(dyn_buffer);
    }

    file.close();

    return true;
}

}
