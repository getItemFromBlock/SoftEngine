#pragma once
#include "IComponent.h"
#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Resource/ComputeShader.h"

class Material;
class Mesh;

struct ParticleData
{
    Vec4f position;
    Vec4f velocity;
    Vec4f color;
    Vec4f padding;
};

struct InstanceData
{
    Vec4f position;
    Vec4f color;
};

class ParticleSystemComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(ParticleSystemComponent);
    
    void Describe(ClassDescriptor& d) override;
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(RHIRenderer* renderer) override;
    void OnDestroy() override;
    
    void SetParticleCount(int count);
    void SetMesh(SafePtr<Mesh> mesh);
    
private:
    void RecreateParticleBuffers();

private:
    std::unique_ptr<ComputeDispatch> m_compute;
    
    std::unique_ptr<RHIBuffer> m_particleBuffer; 
    std::unique_ptr<RHIBuffer> m_instanceBuffer;
    std::unique_ptr<RHIBuffer> m_stagingBuffer;
    
    int m_particleCount = 50000;
    
    SafePtr<Mesh> m_mesh;
    SafePtr<Material> m_material;
    
    bool m_initialUploadComplete = false;
    bool m_needsRecreation = false;
};