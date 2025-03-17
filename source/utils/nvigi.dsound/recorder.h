// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <nvigi_struct.h>
#include <nvigi_stl_helpers.h>

namespace nvigi
{
namespace utils
{

#define NUM_BUFFERS 2
#define BUFFER_SIZE 4096

struct RecordingInfo
{
    std::vector<uint8_t> audioBuffer = {};
    DWORD bytesWritten = 0;
    HWAVEIN hwi;
    WAVEHDR headers[NUM_BUFFERS];
    WAVEFORMATEX waveFormat{};
};

void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (uMsg == WIM_DATA)
    {
        if (dwInstance)
        {
            RecordingInfo& info = *((RecordingInfo*)dwInstance);

            LPWAVEHDR waveHeader = reinterpret_cast<LPWAVEHDR>(dwParam1);
            info.audioBuffer.resize(info.bytesWritten + waveHeader->dwBytesRecorded);
            memcpy(info.audioBuffer.data() + info.bytesWritten, (char*)waveHeader->lpData, waveHeader->dwBytesRecorded);
            info.bytesWritten += waveHeader->dwBytesRecorded;

            // Reuse the buffer for the next recording
            waveInUnprepareHeader(hwi, waveHeader, sizeof(WAVEHDR));
            waveInPrepareHeader(hwi, waveHeader, sizeof(WAVEHDR));
            waveInAddBuffer(hwi, waveHeader, sizeof(WAVEHDR));
        }
    }
}

static std::atomic<bool> isRecording = false;

RecordingInfo* startRecordingAudio()
{
    if (isRecording) return nullptr;

    RecordingInfo* infoPtr = new RecordingInfo;
    RecordingInfo& info = *infoPtr;
    // Open the recording device

    info.waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    info.waveFormat.nChannels = 1;
    info.waveFormat.nSamplesPerSec = 16000;
    info.waveFormat.wBitsPerSample = 16;
    info.waveFormat.cbSize = 0;
    info.waveFormat.nBlockAlign = (info.waveFormat.wBitsPerSample / 8) * info.waveFormat.nChannels;
    info.waveFormat.nAvgBytesPerSec = info.waveFormat.nSamplesPerSec * info.waveFormat.nBlockAlign;
    MMRESULT result = waveInOpen(&info.hwi, WAVE_MAPPER, &info.waveFormat, (DWORD_PTR)waveInProc, (DWORD_PTR)infoPtr, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
    {
        delete infoPtr;
        return nullptr;
    }

    info.bytesWritten = 0;
    info.audioBuffer.clear();

    // Prepare the audio buffers

    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        info.headers[i].lpData = new char[BUFFER_SIZE];
        info.headers[i].dwBufferLength = BUFFER_SIZE;
        info.headers[i].dwBytesRecorded = 0;
        info.headers[i].dwUser = 0;
        info.headers[i].dwFlags = 0;
        info.headers[i].dwLoops = 0;
        result = waveInPrepareHeader(info.hwi, &info.headers[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR)
        {
            delete infoPtr;
            return nullptr;
        }
        result = waveInAddBuffer(info.hwi, &info.headers[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR)
        {
            delete infoPtr;
            return nullptr;
        }
    }

    result = waveInStart(info.hwi);
    if (result != MMSYSERR_NOERROR)
    {
        delete infoPtr;
        return nullptr;
    }
    isRecording.store(true);

    return infoPtr;
}

bool stopRecordingAudio(RecordingInfo* infoPtr, nvigi::InferenceDataAudioSTLHelper* wavData)
{
    if (!infoPtr)
        return false;
    RecordingInfo info = *infoPtr;

    if (!isRecording) return false;
    if (!wavData)
        return false;

    isRecording.store(false);

    waveInStop(info.hwi);

    waveInUnprepareHeader(info.hwi, &info.headers[0], sizeof(WAVEHDR));
    waveInUnprepareHeader(info.hwi, &info.headers[1], sizeof(WAVEHDR));

    delete info.headers[0].lpData;
    delete info.headers[1].lpData;

    waveInClose(info.hwi);

    DWORD dataSize = info.bytesWritten + 36;

    std::vector<int8_t> samples{};
    auto write = [&samples](const char* data, size_t count)->void
    {
        // Append incoming data to the wavData _samples, use std insert or some other simple method
        samples.insert(samples.end(), data, data + count);
    };

    write("RIFF", 4);
    write((char*)&dataSize, 4);
    write("WAVE", 4);
    write("fmt ", 4);
    uint32_t fmtSize = sizeof(WAVEFORMATEX) - sizeof(WORD);
    write((char*)&fmtSize, 4);
    write((char*)&info.waveFormat, fmtSize);
    write("data", 4);
    write((char*)&info.bytesWritten, 4);
    write((char*)info.audioBuffer.data(), info.bytesWritten);

    // Make sure our output slot matches the recored sound format we used here, PCM 16 bit mono, 16kHz
    std::vector<int16_t> samples16(samples.size() / 2);
    memcpy(samples16.data(), samples.data(), samples.size());
    *wavData = samples16;
    
    delete infoPtr;
    return true;
}

}
}