#pragma once
#include <memory>
#include <stdexcept>

template<typename T>
class SafePtr {
private:
    std::weak_ptr<T> weak;

public:
    // Constructors
    SafePtr() = default;
    
    SafePtr(const std::shared_ptr<T>& ptr) : weak(ptr) {}
    
    SafePtr(const std::weak_ptr<T>& ptr) : weak(ptr) {}
    
    // Assignment operators
    SafePtr& operator=(const std::shared_ptr<T>& ptr) {
        weak = ptr;
        return *this;
    }
    
    SafePtr& operator=(const std::weak_ptr<T>& ptr) {
        weak = ptr;
        return *this;
    }
    
    // Check if the pointer is still valid
    bool valid() const {
        return !weak.expired();
    }
    
    explicit operator bool() const {
        return valid();
    }
    
    // Arrow operator - automatically locks and throws if expired
    std::shared_ptr<T> operator->() const {
        auto ptr = weak.lock();
        if (!ptr) {
            throw std::runtime_error("SafePtr: Attempted to access expired pointer");
        }
        return ptr;
    }
    
    // Dereference operator - automatically locks and throws if expired
    T& operator*() const {
        auto ptr = weak.lock();
        if (!ptr) {
            throw std::runtime_error("SafePtr: Attempted to dereference expired pointer");
        }
        return *ptr;
    }
    
    // Safe access that returns nullptr instead of throwing
    std::shared_ptr<T> lock() const {
        return weak.lock();
    }
    
    // Get a temporary shared_ptr (throws if expired)
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
    
    // Reset the SafePtr
    void reset() {
        weak.reset();
    }
    
    // Get use count
    long use_count() const {
        return weak.use_count();
    }
};