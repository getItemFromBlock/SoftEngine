#pragma once
#include <memory>
#include <future>

#include <BS_thread_pool.hpp>

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

    template <typename F, typename R = std::invoke_result_t<std::decay_t<F>>>
    static std::future<R> Enqueue(F&& task)
    {
#ifdef MULTI_THREAD
        try
        {
            auto value =  s_instance->m_threadPool->submit_task(std::forward<F>(task));
            value.get();
            return value;
        }
        catch (const std::exception& e)
        {
            PrintError("ThreadPool: %s", e.what());
            return std::future<R>();
        }
#else
        task();
        return std::future<R>();
#endif
    }

private:
    static std::unique_ptr<ThreadPool> s_instance;
    std::unique_ptr<BS::thread_pool<>> m_threadPool;
};
