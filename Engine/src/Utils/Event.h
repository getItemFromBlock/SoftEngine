#pragma once
#include "EngineAPI.h"
#include <functional>
#include <vector>
#include <mutex>

template<typename... Args>
class ENGINE_API Event {
public:
    Event() = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) = default;
    Event& operator=(Event&&) = default;
    virtual ~Event() = default; 

    using Callback = std::function<void(Args...)>;

    virtual void Bind(Callback callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.push_back(std::move(callback));
    }

    virtual void ClearBindings()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.clear();
    }

    virtual void Invoke(Args... args)
    {
        std::vector<Callback> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callbacksCopy = m_callbacks; // copy under lock
        }

        for (auto& callback : callbacksCopy)
        {
            callback(args...);
        }
    }

    void operator+=(Callback callback)
    {
        Bind(std::move(callback));
    }

    void operator()(Args... args)
    {
        Invoke(args...);
    }

protected:
    std::vector<Callback> m_callbacks;
    mutable std::mutex m_mutex;
};

// Event that runs only once
class ENGINE_API OnceEvent : public Event<> {
public:
    using Event::Event; 
    using Callback = std::function<void()>;

    void Invoke() override
    {
        std::vector<Callback> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_called) return;
            callbacksCopy = m_callbacks;
            m_called = true;
            m_callbacks.clear();
        }

        for (auto& callback : callbacksCopy)
            callback();
    }

    void Bind(Callback callback) override
    {        
        bool callNow = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_called) {
                m_callbacks.push_back(std::move(callback));
            } else {
                callNow = true;
            }
        }

        if (callNow)
            callback();
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_called = false;
    }

private:
    bool m_called = false;
    mutable std::mutex m_mutex;
};
