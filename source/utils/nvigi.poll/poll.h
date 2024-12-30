// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: MIT
//

#pragma once

#include <mutex>
#include <functional>

#include "source/core/nvigi.thread/thread.h"

namespace nvigi::poll
{

template<typename T>
struct PollContext
{
    void signalResultPending()
    {
        std::unique_lock lck(resultPendingMutex);
        resultPending.store(true);
        resultPendingCV.notify_one();
    }

    void signalResultConsumed()
    {
        std::unique_lock lck(resultPendingMutex);
        resultPending.store(false);
        resultPendingCV.notify_one();
    }

    void waitResultPending()
    {
        std::unique_lock lck(resultPendingMutex);
        while (!resultPending)
            resultPendingCV.wait(lck);
    }

    bool checkResultPending()
    {
        std::unique_lock lck(resultPendingMutex);
        return resultPending;
    }

    void waitResultConsumed()
    {
        std::unique_lock lck(resultPendingMutex);
        while (resultPending)
            resultPendingCV.wait(lck);
    }

    T triggerCallback(T state)
    {
        resultPendingStatus.store(state);
        signalResultPending();
        waitResultConsumed();
        return resultPendingStatus.load();
    }

    void schedule(std::function<void(void)> func)
    {
        worker->scheduleWork(func);
    }

    void init(const wchar_t* name, int priority)
    {
        if (!worker)
        {
            worker = new thread::WorkerThread(name, priority);
        }
    }

    void shutdown()
    {
        if (worker)
        {
            worker->flush();
            delete worker;
            worker = nullptr;
        }
    }
    
    bool isInitialized() const
    {
        return worker != nullptr;
    }

    bool flush()
    {
        if (!worker) return false;
        return worker->flush() == std::cv_status::no_timeout;
    }

    Result getResults(bool wait, T* state)
    {
        if (wait)
        {
            waitResultPending();
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

    thread::WorkerThread* worker{};
    std::mutex resultPendingMutex;
    std::condition_variable resultPendingCV{};
    std::atomic<bool> resultPending = false;
    std::atomic<T> resultPendingStatus{};
};

}