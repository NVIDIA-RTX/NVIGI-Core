// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <map>
#include <string>
#if NVIGI_WINDOWS
#include <Windows.h>
#include <tlhelp32.h>
#endif

#include "source/core/nvigi.log/log.h"
#include "source/core/nvigi.api/internal.h"
#include "source/core/nvigi.extra/extra.h"

namespace nvigi
{

namespace extra
{

namespace keyboard
{
struct Keyboard
{
#ifdef NVIGI_WINDOWS
    PROCESSENTRY32 m_processEntry{};
#endif
    std::map<std::string, VirtKey> m_keys;

    inline static Keyboard* s_keyboard = {};
    inline static IKeyboard s_ikeyboard = {};
};

void registerKey(const char* name, const VirtKey& key)
{
    auto& ctx = *Keyboard::s_keyboard;
    if (ctx.m_keys.find(name) == ctx.m_keys.end())
    {
        ctx.m_keys[name] = key;
    }
    else
    {
        NVIGI_LOG_WARN("Hot-key `%s` already registered", name);
    }
}

bool hasFocus();

bool wasKeyPressed(const char* name)
{
    auto& ctx = *Keyboard::s_keyboard;
#ifdef NVIGI_PRODUCTION
    NVIGI_LOG_WARN_ONCE("Keyboard manager disabled in production");
    return false;
#endif
    auto key = ctx.m_keys[name];
    // Only if we have focus, otherwise ignore keys
#ifdef NVIGI_WINDOWS
    if (!hasFocus())
    {
        return false;
    }

    // Table of currently pressed keys with all possible combinations of modifier keys
    // indexed by the virtual key itself and the current state of each modifier key, using
    // 0 if the key is not pressed and 1 if the key is pressed: [vKey][shift][control][alt]
    static bool GKeyDown[256][2][2][2] = { false };
    if (key.m_mainKey <= 0 || key.m_mainKey > 255)
        return false;

    bool bKeyDown = ((GetAsyncKeyState(key.m_mainKey) & 0x8000) != 0) &&
        (((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) == key.m_bShift) &&
        (((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) == key.m_bControl) &&
        (((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) == key.m_bAlt);

    int shiftIndex = key.m_bShift ? 1 : 0;
    int controlIndex = key.m_bControl ? 1 : 0;
    int altIndex = key.m_bAlt ? 1 : 0;

    bool bPressed = !bKeyDown && GKeyDown[key.m_mainKey][shiftIndex][controlIndex][altIndex];
    GKeyDown[key.m_mainKey][shiftIndex][controlIndex][altIndex] = bKeyDown;
    return bPressed;
#else
    return false;
#endif
}

const VirtKey& getKey(const char* name)
{
    auto& ctx = *Keyboard::s_keyboard;
    return ctx.m_keys[name];
}

bool hasFocus()
{
    auto& ctx = *Keyboard::s_keyboard;
#ifdef NVIGI_WINDOWS
    HWND wnd = GetForegroundWindow();
    DWORD pidWindow = 0;
    GetWindowThreadProcessId(wnd, &pidWindow);
    auto pidCurrent = GetCurrentProcessId();
    if (pidCurrent != pidWindow)
    {
        // Check if parent process own the foreground window
        if (!ctx.m_processEntry.dwSize)
        {
            HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            ctx.m_processEntry.dwSize = sizeof(PROCESSENTRY32);
            if (Process32First(h, &ctx.m_processEntry))
            {
                do
                {
                    if (ctx.m_processEntry.th32ProcessID == pidCurrent)
                    {
                        break;
                    }
                } while (Process32Next(h, &ctx.m_processEntry));
            }
            CloseHandle(h);
        }
        return ctx.m_processEntry.th32ParentProcessID == pidWindow;
    }
#endif
    return true;
}

IKeyboard* getInterface()
{
    if (!Keyboard::s_keyboard)
    {
        Keyboard::s_keyboard = new Keyboard();
        Keyboard::s_ikeyboard.getKey = getKey;
        Keyboard::s_ikeyboard.hasFocus = hasFocus;
        Keyboard::s_ikeyboard.registerKey = registerKey;
        Keyboard::s_ikeyboard.wasKeyPressed = wasKeyPressed;
    }
    return &Keyboard::s_ikeyboard;
}
}

}
}
