#include "Texture.h"

#include "Core/Engine.h"
#include "Debug/Log.h"
#include "Loader/ImageLoader.h"
#include "Render/Vulkan/VulkanRenderer.h"

bool Texture::Load(ResourceManager* resourceManager)
{    
    UNUSED(resourceManager);
    m_image = ImageLoader::Image();
    if (!ImageLoader::Load(p_path, m_image))
    {
        PrintError("Failed to load image %s", p_path.generic_string().c_str());
        return false;
    }
    return true;
}

bool Texture::SendToGPU(VulkanRenderer* renderer)
{
    m_buffer = renderer->CreateTexture(m_image);
    ImageLoader::ImageFree(m_image);
    m_image = ImageLoader::Image();
    return true;
}

void Texture::Unload()
{
    m_buffer.reset();
    if (m_image.data)
    {
        ImageLoader::ImageFree(m_image);
        m_image = ImageLoader::Image();
    }
}

void Texture::Describe(ClassDescriptor& descriptor)
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    SafePtr<Texture> self = resourceManager->GetResource<Texture>(p_uuid);
    auto prop = descriptor.AddTexture("Texture", self);
    prop.readOnly = true;
}

void Texture::CreateFromBuffer(const GBufferAttachment& attachment, VkSampler sampler, uint32_t width, uint32_t height)
{
    m_buffer.reset();
    if (!m_buffer)
        m_buffer = std::make_unique<VulkanTexture>();
    
    m_buffer->CreateFromGBuffer(attachment, sampler, width, height);

    p_isLoaded = true;
    EOnLoaded.Invoke();
    
    p_sendToGPU = true;
    EOnSentToGPU.Invoke();
}
