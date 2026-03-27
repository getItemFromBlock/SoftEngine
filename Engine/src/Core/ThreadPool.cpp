#include "ThreadPool.h"

#include "Utils/Platform.h"

std::unique_ptr<ThreadPool> ThreadPool::s_instance = nullptr;

void ThreadPool::Initialize()
{
    s_instance = std::make_unique<ThreadPool>();
    s_instance->m_threadCount = std::thread::hardware_concurrency();
    s_instance->m_threadPool = new std::thread[s_instance->m_threadCount];
    s_instance->m_threadStates = new std::atomic_bool[s_instance->m_threadCount];
    s_instance->m_mainThreadID = std::this_thread::get_id();
    
    for (uint32_t i = 0; i < s_instance->m_threadCount; i++)
    {
        s_instance->m_threadPool[i] = std::thread(&ThreadPool::threadFunc, &(*s_instance), i);
        Platform::SetThreadName(s_instance->m_threadPool[i].native_handle(), ("ThreadPool #" + std::to_string(i)).c_str());
    }
}

void ThreadPool::WaitUntilAllTasksFinished()
{
    bool isEmpty;
    do
    {
        s_instance->m_queueMutex.lock();
        isEmpty = s_instance->m_taskQueue.empty();
        s_instance->m_queueMutex.unlock();
        if (!isEmpty)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (!isEmpty);

    bool isBusy;
    do
    {
        isBusy = false;
        s_instance->m_queueMutex.lock();
        for (uint32_t i = 0; i < s_instance->m_threadCount; i++)
            isBusy |= s_instance->m_threadStates[i];
        s_instance->m_queueMutex.unlock();
        if (isBusy)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (isBusy);
}

float ThreadPool::GetUsage()
{
    float total = 0.0f;
    s_instance->m_queueMutex.lock();
    for (uint32_t i = 0; i < s_instance->m_threadCount; i++)
        if (s_instance->m_threadStates[i]) total++;
    s_instance->m_queueMutex.unlock();
    return total / s_instance->m_threadCount;
}

void ThreadPool::Terminate()
{
    s_instance->m_threadExit = true;
    WaitUntilAllTasksFinished();

    for (uint32_t i = 0; i < s_instance->m_threadCount; i++)
        s_instance->m_threadPool[i].join();
    
    delete[] s_instance->m_threadPool;
    s_instance->m_threadPool = nullptr;
    delete[] s_instance->m_threadStates;
}

void ThreadPool::threadFunc(uint32_t id)
{
    while (!m_threadExit)
    {
        m_queueMutex.lock();
        if (m_taskQueue.empty())
        {
            m_queueMutex.unlock();
            m_threadStates[id] = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        else
        {
            std::function<void()> func = m_taskQueue.front();
            m_taskQueue.pop();
            m_queueMutex.unlock();
            m_threadStates[id] = true;
            func();
        }
    }
    m_threadStates[id] = false;
}
