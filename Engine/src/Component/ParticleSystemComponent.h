#pragma once
#include "IComponent.h"
#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Resource/ComputeShader.h"
#include "Utils/Random.h"

class Material;
class Mesh;

template <typename T>
struct MinMax
{
    T min;
    T max;

    MinMax() = default;

    MinMax(T defaultValue)
    {
        min = max = defaultValue;
    }

    T RandomValue(Seed seed) const;
};


template <typename T>
struct ParticleProperty
{
    MinMax<T> value;

    enum class Type
    {
        Constant,
        Random
    } type = Type::Constant;

    ParticleProperty() = default;

    ParticleProperty(T defaultValue)
    {
        value = defaultValue;
    }

    T GetValue(Seed seed) const
    {
        switch (type)
        {
        case Type::Constant:
            return value.min;
        case Type::Random:
            return value.RandomValue(seed);
        }
        return {};
    }
};

struct ParticleSettings
{
    struct General
    {
        float duration = 5.f;
        bool looping = true;
        bool preWarm = true;
        ParticleProperty<float> startDelay = 0.f;
        ParticleProperty<float> startLifeTime = 5.f;
        ParticleProperty<float> startSpeed = 5.f;
        ParticleProperty<float> startSize = 1.f;

        int particleCount = 1000;

        ParticleProperty<Vec4f> startColor = Vec4f::One();
    } general;

    struct Emission
    {
        ParticleProperty<float> rateOverTime = 10.f;
    } emission;

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
            return "None\0Sphere";
        }
        
        float radius = 1.f;
        
    } shape;

    struct Rendering
    {
        bool billboard = false;
    } rendering;
};

struct ParticleData
{
    Vec4f positionSize;
    Vec4f velocityLifeTime;
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

    void ApplySettings();
    ParticleSettings& GetSettings() { return m_particleSettings; }

    // Playback
    bool IsPlaying() const { return m_isPlaying; }
    void Play();
    void Pause();
    void Restart();
    float GetPlaybackTime() const { return m_currentTime; }
    void SetPlaybackTime(float time) { m_currentTime = time; }

    void SetBillboard(bool enable);
private:
    void RecreateParticleBuffers();
    void InitializeParticleData(ParticleData& p, uint32_t index) const;

    void ReadbackDebugData();
private:
    std::unique_ptr<ComputeDispatch> m_compute;

    std::unique_ptr<RHIBuffer> m_particleBuffer;
    std::unique_ptr<RHIBuffer> m_instanceBuffer;
    std::unique_ptr<RHIBuffer> m_stagingBuffer;

    SafePtr<Mesh> m_mesh;
    SafePtr<Material> m_material;

    bool m_initialUploadComplete = false;
    bool m_needsRecreation = false;
    bool m_needsShaderChange = false;

    bool m_debugReadbackEnabled = true;
    std::unique_ptr<VulkanBuffer> m_debugReadbackBuffer;
    std::vector<ParticleData> m_debugCPUBuffer;

    Seed m_seed;
    ParticleSettings m_particleSettings;
    bool m_isPlaying = true;
    float m_currentTime = 0.f;
};
