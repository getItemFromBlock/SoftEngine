#pragma once

class Window;

class RHIContext
{
public:
    RHIContext() = default;
    RHIContext& operator=(const RHIContext& other) = default;
    RHIContext(const RHIContext&) = default;
    RHIContext(RHIContext&&) noexcept = default;
    virtual ~RHIContext() = default;

    virtual bool Initialize(Window* window) = 0;
    virtual void Cleanup() = 0;
};
