#pragma once
#include <iostream>
#include <unordered_map>

struct UBOBinding
{
    int set;
    int binding;
    
    UBOBinding(int set, int binding) : set(set), binding(binding) {}
    UBOBinding(uint32_t set, uint32_t binding) : UBOBinding(static_cast<int>(set), static_cast<int>(binding)) {}
    UBOBinding() : UBOBinding(0, 0) {}
    
    bool operator==(const UBOBinding& other) const noexcept
    {
        return set == other.set && binding == other.binding;
    }
};

namespace std
{
    template<>
    struct hash<UBOBinding>
    {
        std::size_t operator()(const UBOBinding& k) const noexcept
        {
            std::size_t h1 = std::hash<int>()(k.set);
            std::size_t h2 = std::hash<int>()(k.binding);

            std::size_t seed = h1;
            seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}


class RHIUniformBuffer
{
public:
    virtual ~RHIUniformBuffer() = default;
    
};

using UniformBuffersOwner = std::unordered_map<UBOBinding, std::unique_ptr<RHIUniformBuffer>>;
using UniformBuffers = std::unordered_map<UBOBinding, RHIUniformBuffer*>;