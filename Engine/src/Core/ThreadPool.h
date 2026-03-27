#pragma once
#include <memory>
#include <future>
#include <queue>

#include "Debug/Log.h"

class ThreadPool
{
public:
    ThreadPool() = default;
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ~ThreadPool() = default;

    static void Initialize();
    static void WaitUntilAllTasksFinished();
    static void Terminate();
    static float GetUsage();
    
    static bool IsMainThread() { return std::this_thread::get_id() == s_instance->m_mainThreadID; }

    static void Enqueue(std::function<void()> task)
    {
#ifdef MULTI_THREAD
        s_instance->m_queueMutex.lock();
        s_instance->m_taskQueue.push(task);
        s_instance->m_queueMutex.unlock();
#else
        task();
#endif
    }

private:
    void threadFunc(uint32_t id);

private:
    static std::unique_ptr<ThreadPool> s_instance;
    std::thread *m_threadPool;
    std::atomic_bool *m_threadStates;
    std::atomic_bool m_threadExit = false;
    uint32_t m_threadCount;
    std::queue<std::function<void()>> m_taskQueue;
    std::mutex m_queueMutex;
    std::thread::id m_mainThreadID;
};
