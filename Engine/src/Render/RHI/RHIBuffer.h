#pragma once

class RHIBuffer
{
public:
    RHIBuffer() = default;
    RHIBuffer& operator=(const RHIBuffer& other) = default;
    RHIBuffer(const RHIBuffer&) = default;
    RHIBuffer(RHIBuffer&&) noexcept = default;
    virtual ~RHIBuffer() = default;
    
    virtual void Cleanup() {}
};
