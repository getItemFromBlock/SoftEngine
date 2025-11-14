#pragma once

class RHITexture
{
public:
    RHITexture() = default;
    RHITexture& operator=(const RHITexture& other) = default;
    RHITexture(const RHITexture&) = default;
    RHITexture(RHITexture&&) noexcept = default;
    ~RHITexture() = default;
};
