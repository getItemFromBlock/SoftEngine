#pragma once
#include "IComponent.h"
#include "Render/Vulkan/VulkanBuffer.h"
#include "Resource/ComputeShader.h"

class ParticleSystemComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(ParticleSystemComponent);
    
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    
private:
    std::unique_ptr<ComputeDispatch> m_compute;
    
    std::unique_ptr<VulkanBuffer> m_gpuBuffer;
    std::unique_ptr<VulkanBuffer> m_stagingBuffer;
    
};
