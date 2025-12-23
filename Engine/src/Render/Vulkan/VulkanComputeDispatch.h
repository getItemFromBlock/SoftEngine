#pragma once
#include "VulkanDescriptorPool.h"
#include "Render/RHI/RHIComputeDispatch.h"

class Shader;

class VulkanComputeDispatch : public RHIComputeDispatch
{
public:
    VulkanComputeDispatch() = default;
    VulkanComputeDispatch(const VulkanComputeDispatch&) = delete;
    VulkanComputeDispatch(VulkanComputeDispatch&&) = delete;
    VulkanComputeDispatch& operator=(const VulkanComputeDispatch&) = delete;
    virtual ~VulkanComputeDispatch() = default;
    
    // bool Initialize(VulkanDevice* device, Shader* shader);
private:
    std::unique_ptr<VulkanDescriptorPool> m_descriptorPool;
};
