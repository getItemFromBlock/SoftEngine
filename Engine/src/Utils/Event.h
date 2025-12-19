#pragma once
#include "EngineAPI.h"
#include <functional>
#include <vector>
#include <mutex>
#pragma once
#include "EngineAPI.h"
#include <functional>
#include <vector>
#include <mutex>
#include <cstdint>
#include <algorithm>

using EventHandle = uint64_t;

template<typename... Args>
class ENGINE_API Event {
public:
    using Callback = std::function<void(Args...)>;

    Event() = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) = default;
    Event& operator=(Event&&) = default;
    virtual ~Event() = default;

    virtual EventHandle Bind(Callback callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        EventHandle id = ++m_nextId;
        m_callbacks.push_back({ id, std::move(callback) });
        return id;
    }

    virtual void Unbind(EventHandle id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::erase_if(m_callbacks, [id](const Entry& e) { return e.id == id; });
    }

    void ClearBindings()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.clear();
    }

    virtual void Invoke(Args... args)
    {
        std::vector<Entry> copy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            copy = m_callbacks;
        }

        for (auto& e : copy)
            e.callback(args...);
    }

    void operator()(Args... args)
    {
        Invoke(args...);
    }

    EventHandle operator+=(Callback callback)
    {
        return Bind(callback);    
    }
    
protected:
    struct Entry {
        EventHandle id;
        Callback callback;
    };

    std::vector<Entry> m_callbacks;
    EventHandle m_nextId = 0;
    mutable std::mutex m_mutex;
};

// Event that run only once (when bind, call the method if already call)
class ENGINE_API OnceEvent : public Event<> {
public:
    using Callback = std::function<void()>;

    EventHandle Bind(Callback callback) override
    {
        bool callNow = false;
        EventHandle id = 0;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_called) {
                id = ++m_nextId;
                m_callbacks.push_back({ id, std::move(callback) });
            } else {
                callNow = true;
            }
        }

        if (callNow)
            callback();

        return id;
    }

    void Unbind(EventHandle id) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::erase_if(m_callbacks, [id](const Entry& e) { return e.id == id; });
    }

    void Invoke() override
    {
        std::vector<Entry> copy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_called)
                return;

            m_called = true;
            copy = m_callbacks;
            m_callbacks.clear();
        }

        for (auto& e : copy)
            e.callback();
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_called = false;
    }

private:
    bool m_called = false;
};
