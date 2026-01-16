#pragma once
#include "IComponent.h"

#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"

#include "Resource/ComputeShader.h"

#include "Utils/Random.h"

class Material;
class Mesh;

struct ParticleSettings
{
    struct General
    {
        Vec3i particleAmount;
        uint32_t solidLayers;
        Vec2i boneDensity;
        Vec2i surfaceDensity;
        Vec2f surfaceHeightBounds;
    } general;

    struct Shape
    {
        enum class Type
        {
            None,
            Sphere,
            Cube,
            Cone,
        } type = Type::Sphere;

        static const char* to_cstr()
        {
            return "None\0Sphere\0Cube\0Cone";
        }
        
        float radius = 1.f;
        
    } shape;
};

struct ParticleData
{
    Vec3f position;
    uint32_t connectionsOffset;
    Vec3f velocity;
    uint32_t connectionsCount;
};

struct ConnectionData
{
    uint32_t particleID;
    float initialLength;
};

struct InstanceData
{
    Vec3f localPosition;
    float localScale;
    Quat  localRotation;
};

class GPUSoftBodyComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(GPUSoftBodyComponent);

    void Describe(ClassDescriptor& d) override;
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VulkanRenderer* renderer) override;
    void OnDestroy() override;

    void ApplySettings();
    ParticleSettings& GetSettings() { return m_particleSettings; }

    void Restart();

    void SetBillboard(bool enable);
    
    SafePtr<Material> GetMaterial() const { return m_material; }
    SafePtr<Mesh> GetMesh() const { return m_mesh; }
private:
    void RecreateParticleBuffers();
    void InitializeParticleData(ParticleData& p, uint32_t index) const;

    void ReadbackDebugData();
private:
    std::unique_ptr<ComputeDispatch> m_simulationCompute0;
    std::unique_ptr<ComputeDispatch> m_simulationCompute1;

    std::unique_ptr<VulkanBuffer> m_particleBuffer;
    std::unique_ptr<VulkanBuffer> m_instanceBuffer;

    SafePtr<Mesh> m_mesh;
    SafePtr<Material> m_material;

    bool m_initialUploadComplete = false;
    bool m_needsRecreation = false;
    bool m_needsShaderChange = false;

    bool m_debugReadbackEnabled = false;
    std::unique_ptr<VulkanBuffer> m_debugReadbackBuffer;
    std::vector<ParticleData> m_debugCPUBuffer;

    Seed m_seed;
    ParticleSettings m_particleSettings;
};
