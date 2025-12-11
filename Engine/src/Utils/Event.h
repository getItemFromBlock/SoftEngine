#pragma once
#include <functional>

template<typename... Args>
class Event {
public:
	Event() = default;
	Event& operator=(const Event& other) = default;
	Event(const Event&) = default;
	Event(Event&&) noexcept = default;
	virtual ~Event() = default; 

	using Callback = std::function<void(Args...)>;

	virtual void Bind(Callback callback)
	{
		m_callbacks.push_back(callback);
	}

	virtual void ClearBindings()
	{
		m_callbacks.clear();
	}

	virtual void Invoke(Args... args)
	{
		for (auto& callback : m_callbacks)
		{
			callback(args...);
		}
	}
	
	void operator+=(Callback callback)
	{
		Bind(callback);
	}
	
	void operator()(Args... args)
	{
		Invoke(args...);
	}
private:
	std::vector<Callback> m_callbacks;
};

// Event that run only once
class OnceEvent : public Event<>
{
public:
	using Event::Event; 

	using Callback = std::function<void()>;
	
	void Invoke() override
	{
		Event::Invoke();
		m_called = true;
		ClearBindings();
	}

	void Bind(Callback callback) override
	{		
		Event::Bind(callback);
		if (m_called)
			Invoke();
	}
	
	void Reset()
	{
		m_called = false;
	}
private:
	bool m_called = false;
};