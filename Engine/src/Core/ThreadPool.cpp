#include "ThreadPool.h"

#include "Utils/Platform.h"

std::unique_ptr<ThreadPool> ThreadPool::s_instance = nullptr;
void ThreadPool::Initialize()
{
    s_instance = std::make_unique<ThreadPool>();
    s_instance->m_threadPool = std::make_unique<BS::thread_pool<>>(std::thread::hardware_concurrency());
    s_instance->m_mainThreadID = std::this_thread::get_id();

    auto ids = s_instance->m_threadPool->get_thread_ids();
    for (size_t i = 0; i < std::thread::hardware_concurrency(); i++)
    {
        uint32_t threadId = static_cast<uint32_t>(std::hash<std::thread::id>{}(ids[i]));
        Platform::SetThreadName(threadId, ("ThreadPool #" + std::to_string(i)).c_str());
    }
}

void ThreadPool::WaitUntilAllTasksFinished()
{
    s_instance->m_threadPool->wait();
}

void ThreadPool::Terminate()
{
    s_instance->m_threadPool->reset();
    s_instance->m_threadPool.reset();
}
