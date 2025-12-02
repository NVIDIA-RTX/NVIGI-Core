// SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <mutex>
#include <functional>
#include <chrono>

#include "source/core/nvigi.thread/thread.h"

namespace nvigi::poll
{

constexpr uint32_t kDefaultTimeoutMs = 5000;

template<typename T>
struct PollContext
{
    void signalResultPending()
    {
        std::unique_lock lck(resultPendingMutex);
        resultPending = true;
        resultPendingCV.notify_one();
    }

    void signalResultConsumed()
    {
        std::unique_lock lck(resultPendingMutex);
        resultPending = false;
        resultPendingCV.notify_one();
    }

    Result waitResultPending(uint32_t timeoutMs = kDefaultTimeoutMs)
    {
        std::unique_lock lck(resultPendingMutex);
        if (!resultPendingCV.wait_for(lck, std::chrono::milliseconds(timeoutMs), [this]() { return resultPending; }))
        {
            return nvigi::kResultTimedOut;
        }
        return nvigi::kResultOk;
    }

    bool checkResultPending()
    {
        std::unique_lock lck(resultPendingMutex);
        return resultPending;
    }

    T triggerCallback(T state)
    {
        resultPendingStatus.store(state);
        signalResultPending();
        // Wait indefinitely for host to consume results (no timeout)
        std::unique_lock lck(resultPendingMutex);
        resultPendingCV.wait(lck, [this]() { return !resultPending; });
        return resultPendingStatus.load();
    }

    
    Result getResults(bool wait, T* state, uint32_t timeoutMs = kDefaultTimeoutMs)
    {
        if (wait)
        {
            Result result = waitResultPending(timeoutMs);
            if (result != nvigi::kResultOk)
                return result;
        }
        else
        {
            if (!checkResultPending())
                return nvigi::kResultNotReady;
        }
        if (state)
            *state = resultPendingStatus;
        return kResultOk;
    }

    Result releaseResults(T state)
    {
        resultPendingStatus = state;
        signalResultConsumed();
        return kResultOk;
    }

    std::mutex resultPendingMutex;
    std::condition_variable resultPendingCV{};
    bool resultPending = false;
    std::atomic<T> resultPendingStatus{};
};

}