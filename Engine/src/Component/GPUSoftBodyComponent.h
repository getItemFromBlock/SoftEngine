#pragma once
#include "IComponent.h"

#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"

#include "Resource/ComputeShader.h"

#include "Utils/Random.h"

#include <vector>

class Material;
class Mesh;
class SceneSerializer;

struct BodySettings
{
    struct General
    {
        Vec3i particleAmount = Vec3i(11, 11, 11);
        int32_t solidLayers = 1;
        Vec2i boneCount = Vec2i(4, 4);
        Vec2i surfacePoints = Vec2i(8, 8);
        Vec2f surfaceHeightBounds = Vec2f(-0.3f, 0.3f);
        float damping = 1.0f;
        float strength = 300.0f;
        uint32_t connectionStrength = 1.5;
    } general;

    struct Shape
    {
        enum class Type : int32_t
        {
            Cube,
            Sphere,
            Cone,
        } type = Type::Cube;

        static const char* to_cstr()
        {
            return "Cube\0Sphere\0Cone\0";
        }
        
        float scale = 1.f;
        
    } shape;
};

struct SBParticleData
{
    Vec3f position;
    uint32_t connectionsOffset;
    Vec3f velocity;
    uint32_t connectionsCount;
    Vec3f originalPos;
    float unused;
};

struct ConnectionData
{
    uint32_t particleID;
    float initialLength;
};

struct SoftBodyInstanceData
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
    void OnGameUpdate(float deltaTime) override;
    void OnRender(VulkanRenderer* renderer) override;
    void OnDestroy() override;

    void ApplySettings();
    BodySettings& GetSettings() { return m_particleSettings; }
    
    void CreateFromModel(SafePtr<Model> inputModel);

    bool IsLoadedFromModel() const { return m_loadedFromModel; }
    SafePtr<Model> GetInitializerModel() const { return m_initializerModel; }
    
    bool GetDrawDebug() const { return m_drawDebug; }
    void SetDrawDebug(bool drawDebug) { m_drawDebug = drawDebug; }
    
    std::vector<SafePtr<Material>> GetMaterial() const { return m_materials; }
    SafePtr<Mesh> GetMesh() const { return m_mesh; }
private:
    friend class SceneSerializer;

    void ReleaseGPUResources(std::unique_ptr<VulkanBuffer> particleBuffer,
        std::shared_ptr<Mesh> mesh = {},
        std::unique_ptr<ComputeDispatch> simulationCompute0 = {},
        std::unique_ptr<ComputeDispatch> simulationCompute1 = {});
    
    void CreateParticleBuffers();
    void CreateSkinnedMesh(std::vector<WeightedVertex> &vertices, std::vector<uint32_t> &indices);
    void MapMeshToParticles(std::vector<WeightedVertex> &vertices);
    void InitializeParticleData(std::vector<SBParticleData> &particles, std::vector<ConnectionData> &connections);
    // Pwease dwo not caww at wuntiwe, i am a sweepy method OwO
    void InitializeParticleDataFromModel(float density, float maxDistToConnect);
    void InitializeMaterialsFromModel(SafePtr<Model> inputModel);

    // Generation from meshes methods

    void PlacePointMesh(BoundingBox BBox, Vertex* vertices, uint32_t* indices, int pointCount, float density);

    void GenerateConnection(const BoundingBox& BBox, const float& maxDistToConnect);

private:
    bool m_loadedFromModel = false;

    SafePtr<Model>  m_initializerModel;
    std::vector<SafePtr<Material>> m_materials;


    std::unique_ptr<ComputeDispatch> m_simulationCompute0;
    std::unique_ptr<ComputeDispatch> m_simulationCompute1;

    std::unique_ptr<VulkanBuffer> m_particleBuffer;
    // Size of GPU buffer section reserved for particle data
    VkDeviceSize PBufSizeAligned;
    // Size of GPU buffer section reserved for particle connections, located right after particle data in memory
    VkDeviceSize CBufSizeAligned;
    uint32_t m_totalParticleCount = 0;

    std::shared_ptr<Mesh> m_mesh;
    SafePtr<Mesh> m_billboardMesh;
    SafePtr<Material> m_defaultMaterial;
    SafePtr<Material> m_billboardMaterial;

    std::vector<SBParticleData> m_particles;
    std::vector<ConnectionData> m_connections;

    bool m_needsRecreation = false;
    bool m_drawDebug = false;

    Seed m_seed;
    BodySettings m_particleSettings;

    float m_meshDensity;
};
