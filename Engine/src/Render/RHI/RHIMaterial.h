#pragma once
#include <iostream>

class RHIRenderer;
class RHIMaterial
{
public:
    RHIMaterial() = default;
    RHIMaterial(const RHIMaterial&) = delete;
    RHIMaterial(RHIMaterial&&) = delete;
    RHIMaterial& operator=(const RHIMaterial&) = delete;
    virtual ~RHIMaterial() = default;
    
    virtual void Bind(RHIRenderer* renderer) {}
    virtual void SetUniformData(uint32_t set, uint32_t binding, const void* data, size_t size, RHIRenderer* renderer) {}
    virtual void Cleanup() {}
};
