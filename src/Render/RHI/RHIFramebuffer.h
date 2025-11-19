#pragma once

class RHIFramebuffer
{
public:
    RHIFramebuffer() = default;
    RHIFramebuffer& operator=(const RHIFramebuffer& other) = default;
    RHIFramebuffer(const RHIFramebuffer&) = default;
    RHIFramebuffer(RHIFramebuffer&&) noexcept = default;
    virtual ~RHIFramebuffer() = default;
    
};
