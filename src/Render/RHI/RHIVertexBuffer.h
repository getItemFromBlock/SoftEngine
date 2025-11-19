#pragma once

class RHIVertexBuffer
{
public:
    RHIVertexBuffer() = default;
    RHIVertexBuffer& operator=(const RHIVertexBuffer& other) = default;
    RHIVertexBuffer(const RHIVertexBuffer&) = default;
    RHIVertexBuffer(RHIVertexBuffer&&) noexcept = default;
    virtual ~RHIVertexBuffer() = default;
    
    
};
