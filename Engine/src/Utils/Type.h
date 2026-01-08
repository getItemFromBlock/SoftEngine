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
    
    SafePtr& operator=(const std::shared_ptr<T>& ptr) {
        weak = ptr;
        return *this;
    }
    
    SafePtr& operator=(const std::weak_ptr<T>& ptr) {
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
};