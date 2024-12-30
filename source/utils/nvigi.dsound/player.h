// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dsound.h>
#include <mmreg.h>

namespace nvigi
{
namespace utils
{

struct Player
{
    static constexpr uint32_t DEFAULT_SAMPLING_RATE{ 22050u };

    Player(uint32_t bitsPerSample = 32, uint32_t sample_rate = DEFAULT_SAMPLING_RATE);
    Player(const Player&) = delete;
    Player(Player&&) = delete;
    virtual ~Player();

    std::atomic_bool initialized{ false };
    LPDIRECTSOUND ds{ nullptr };
    WAVEFORMATEXTENSIBLE waveFormat{};
};

struct Buffer
{
    Buffer(Player& player, const void* const buffer, const DWORD buffer_size);
    Buffer(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    virtual ~Buffer();

    void Play();
    void Wait();

    std::atomic_bool initialized{ false };
    LPDIRECTSOUNDBUFFER buf;
    DSBUFFERDESC desc = { sizeof(DSBUFFERDESC) };
};

Player::Player(uint32_t bitsPerSample, uint32_t sample_rate)
{
    HRESULT res{ S_FALSE };

    res = DirectSoundCreate(NULL, &ds, NULL);
    if (res != S_OK) { return; }

    res = ds->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
    if (res != S_OK) { return; }

    // Set the wave format parameters
    waveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE; // Use extensible format
    waveFormat.Format.nChannels = 1; // Mono
    waveFormat.Format.nSamplesPerSec = sample_rate; // default sample rate
    waveFormat.Format.wBitsPerSample = bitsPerSample; // 16 bits per sample
    waveFormat.Format.nBlockAlign = (waveFormat.Format.nChannels * waveFormat.Format.wBitsPerSample) / 8; // Block size of data
    waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign; // For buffer estimation
    waveFormat.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX); // Size of the extra information

    waveFormat.Samples.wValidBitsPerSample = waveFormat.Format.wBitsPerSample; // All bits are valid for PCM

    // Set the subformat GUID for PCM audio or float
    waveFormat.SubFormat = bitsPerSample == 16 ? KSDATAFORMAT_SUBTYPE_PCM : KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    initialized = true;
}

Player::~Player()
{
    if (initialized)
    {
        ds->Release();
        initialized = false;
    }
}

Buffer::Buffer(Player& player, const void* const buffer, const DWORD buffer_size)
{
    HRESULT res{ S_FALSE };

    if (!player.initialized || !player.ds)
        return;

    desc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
    desc.dwBufferBytes = buffer_size;
    desc.lpwfxFormat = (LPWAVEFORMATEX)&player.waveFormat;
    res = player.ds->CreateSoundBuffer(&desc, &buf, NULL);
    if (res != S_OK) { return; }

    void* ptr1;
    DWORD len1;
    res = buf->Lock(0, buffer_size, &ptr1, &len1, 0, 0, 0);
    if (res != S_OK) { return; }

    if (!(memcpy(ptr1, buffer, buffer_size))) { return; }

    buf->Unlock(ptr1, len1, 0, 0);
    if (res != S_OK) { return; }

    initialized = true;
}

Buffer::~Buffer()
{
    if (initialized)
    {
        buf->Release();
        initialized = false;
    }
}

void Buffer::Play()
{
    if (initialized)
    {
        buf->Play(0, 0, 0);
    }
}

void Buffer::Wait()
{
    if (initialized)
    {
        DWORD status;
        do
        {
            if (buf->GetStatus(&status) != S_OK) { return; }
            Sleep(0);
        } while (status & DSBSTATUS_PLAYING);
    }
}

}
}