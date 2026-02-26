#pragma once
#include <memory>
#include <stdexcept>

template<typename T, typename U>
T* Cast(U* ptr)
{
    return dynamic_cast<T*>(ptr);
}

template<typename T>
class SafePtr {
private:
    std::weak_ptr<T> weak;

public:
    // Constructors
    SafePtr() = default;
    
    SafePtr(const std::shared_ptr<T>& ptr) : weak(ptr) {}
    
    SafePtr(const std::weak_ptr<T>& ptr) : weak(ptr) {}
    
    // Templated conversion constructor for derived-to-base conversion
    template<typename U>
    SafePtr(const SafePtr<U>& other) requires (std::is_convertible_v<U*, T*>) : weak(other.weak) {}
    
    template<typename U>
    SafePtr(const std::shared_ptr<U>& ptr) requires (std::is_convertible_v<U*, T*>) : weak(ptr) {}
    
    template<typename U>
    SafePtr(const std::weak_ptr<U>& ptr) requires (std::is_convertible_v<U*, T*>) : weak(ptr) {}
    
    ~SafePtr()
    {
        weak.reset();
    }
    
    SafePtr& operator=(const std::shared_ptr<T>& ptr) {
        weak = ptr;
        return *this;
    }
    
    SafePtr& operator=(const std::weak_ptr<T>& ptr) {
        weak = ptr;
        return *this;
    }
    
    // Templated assignment operators for derived-to-base conversion
    template<typename U>
    SafePtr& operator=(const SafePtr<U>& other) requires (std::is_convertible_v<U*, T*>)
    {
        weak = other.weak;
        return *this;
    }
    
    template<typename U>
    SafePtr& operator=(const std::shared_ptr<U>& ptr) requires (std::is_convertible_v<U*, T*>)
    {
        weak = ptr;
        return *this;
    }
    
    template<typename U>
    SafePtr& operator=(const std::weak_ptr<U>& ptr) requires (std::is_convertible_v<U*, T*>)
    {
        weak = ptr;
        return *this;
    }
    
    bool valid() const {
        return !weak.expired();
    }
    
    explicit operator bool() const {
        return valid();
    }
    
    std::shared_ptr<T> operator->() const {
        auto ptr = weak.lock();
        if (!ptr) {
            throw std::runtime_error("SafePtr: Attempted to access expired pointer");
        }
        return ptr;
    }
    
    T& operator*() const {
        auto ptr = weak.lock();
        if (!ptr) {
            throw std::runtime_error("SafePtr: Attempted to dereference expired pointer");
        }
        return *ptr;
    }
    
    std::shared_ptr<T> lock() const {
        return weak.lock();
    }
    
    std::shared_ptr<T> get() const {
        auto ptr = weak.lock();
        if (!ptr) {
            throw std::runtime_error("SafePtr: Pointer has expired");
        }
        return ptr;
    }
    
    T* getPtr() const {
        return weak.lock().get();
    }
    
    void reset() {
        weak.reset();
    }
    
    long use_count() const {
        return weak.use_count();
    }
    
    // Allow other SafePtr instantiations to access private members
    template<typename U>
    friend class SafePtr;
};