#pragma once
#include <memory>

#include <BS_thread_pool.hpp>

// #define MULTI_THREAD
class ThreadPool
{
public:
    ThreadPool() = default;
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ~ThreadPool() = default;

    static void Initialize();
    static void Terminate();

    template <typename F, typename R = std::invoke_result_t<std::decay_t<F>>>
    static std::future<R> Enqueue(F&& task)
    {
#ifdef MULTI_THREAD
        return s_instance->m_threadPool->submit_task(std::forward<F>(task));
#else
        task();
        return {};
#endif
    }

private:
    static std::unique_ptr<ThreadPool> s_instance;
    std::unique_ptr<BS::thread_pool<>> m_threadPool;
};
