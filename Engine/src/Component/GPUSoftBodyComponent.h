#pragma once
#include "IComponent.h"

#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"

#include "Resource/ComputeShader.h"

#include "Utils/Random.h"

#include <vector>

class Material;
class Mesh;

struct ParticleSettings
{
    struct General
    {
        Vec3i particleAmount = Vec3i(10, 10, 10);
        uint32_t solidLayers = 1;
        Vec2i boneCount = Vec2i(4, 4);
        Vec2i surfacePoints = Vec2i(32, 32);
        Vec2f surfaceHeightBounds = Vec2f(-0.3f, 0.3f);
        uint32_t connectionStrength = 2;
    } general;

    struct Shape
    {
        enum class Type
        {
            Cube,
            Sphere,
            Cone,
        } type = Type::Sphere;

        static const char* to_cstr()
        {
            return "Cube\0Sphere\0Cone";
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
    
    SafePtr<Material> GetMaterial() const { return m_material; }
    SafePtr<Mesh> GetMesh() const { return m_mesh; }
private:
    void CreateParticleBuffers();
    void InitializeParticleData(std::vector<ParticleData> &particles, std::vector<ConnectionData> &connections);

private:
    std::unique_ptr<ComputeDispatch> m_simulationCompute0;
    std::unique_ptr<ComputeDispatch> m_simulationCompute1;

    std::unique_ptr<VulkanBuffer> m_particleBuffer;
    // Size of GPU buffer section reserved for particle data
    VkDeviceSize PBufSizeAligned;
    // Size of GPU buffer section reserved for particle connections, located right after particle data in memory
    VkDeviceSize CBufSizeAligned;
    uint32_t totalParticleCount = 0;

    SafePtr<Mesh> m_mesh;
    SafePtr<Material> m_material;

    bool m_initialUploadComplete = false;
    bool m_needsRecreation = false;

    Seed m_seed;
    ParticleSettings m_particleSettings;
};
