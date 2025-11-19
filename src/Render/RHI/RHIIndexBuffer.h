#pragma once

class RHIIndexBuffer
{
public:
    RHIIndexBuffer() = default;
    RHIIndexBuffer& operator=(const RHIIndexBuffer& other) = default;
    RHIIndexBuffer(const RHIIndexBuffer&) = default;
    RHIIndexBuffer(RHIIndexBuffer&&) noexcept = default;
    virtual ~RHIIndexBuffer() = default;
    
};
