#include "CubeMap.h"

#include "ComputeShader.h"
#include "Core/Engine.h"
#include "Debug/Log.h"
#include "Loader/ImageLoader.h"
#include "Render/Vulkan/VulkanRenderer.h"

bool CubeMap::Load(ResourceManager* resourceManager)
{
    if (p_path.extension() == ".hdr")
    {
        if (!ImageLoader::LoadHDR(p_path, m_image))
        {
            PrintError("Failed to load cube map %s", p_path.generic_string().c_str());
            return false;
        }
    }
    else
    {
        PrintError("Not handled yet");
    }
    return true;
}
bool CubeMap::SendToGPU(VulkanRenderer* renderer)
{
    m_buffer = renderer->CreateCubeMap(m_image);
    ImageLoader::ImageFree(m_image);
    m_image = ImageLoader::HDRImage();
   
    GenerateIrradiance(renderer, 512);
    return true;
}
bool CubeMap::GenerateIrradiance(VulkanRenderer* renderer, uint32_t resolution)
{
    if (!m_buffer)
    {
        PrintError("CubeMap: Cannot generate irradiance, source cubemap not on GPU yet");
        return false;
    }

    m_irradianceMap = std::make_unique<VulkanTexture>();
    if (!m_irradianceMap->CreateCubemap(resolution, renderer->GetDevice(),
                                         renderer->GetCommandPool(),
                                         renderer->GetDevice()->GetGraphicsQueue()))
    {
        PrintError("CubeMap: Failed to create irradiance cubemap");
        return false;
    }

    auto resourceManager = Engine::Get()->GetResourceManager();
    SafePtr<Shader> irradianceShader = resourceManager->Load<Shader>(
        RESOURCE_PATH"shaders/IrradianceCompute/irradiance.shader");

    irradianceShader->EOnSentToGPU.Bind([this, renderer, resolution, irradianceShader]()
    {
        m_irradianceCompute = irradianceShader->CreateDispatch(renderer);
        auto device = renderer->GetDevice();
        VulkanMaterial* mat = m_irradianceCompute->GetMaterial();

        // Bind source environment cubemap
        mat->SetTextureCube(0, 0,
            m_buffer->GetImageView(),
            m_buffer->GetSampler(),
            renderer);

        VkImageView allFacesView = m_irradianceMap->GetMipLevelView(0);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView   = allFacesView;
        imageInfo.sampler     = VK_NULL_HANDLE;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = mat->GetDescriptorSet(0)->GetDescriptorSet(renderer->GetFrameIndex());
        write.dstBinding      = 1;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo;
        vkUpdateDescriptorSets(device->GetDevice(), 1, &write, 0, nullptr);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = renderer->GetCommandPool()->GetCommandPool();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        mat->GetPipeline()->Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
        mat->BindForCompute(cmd, renderer->GetFrameIndex());

        uint32_t groups = (resolution + 7) / 8;

        for (int face = 0; face < 6; ++face)
        {
            struct PushConstants {
                int face;
                int resolution;
            } pc{ face, static_cast<int>(resolution) };

            vkCmdPushConstants(cmd,
                mat->GetPipeline()->GetPipelineLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT,
                0, sizeof(PushConstants), &pc);

            vkCmdDispatch(cmd, groups, groups, 1);

            if (face < 5)
            {
                VkMemoryBarrier memBarrier{};
                memBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                memBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 1, &memBarrier, 0, nullptr, 0, nullptr);
            }
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = m_irradianceMap->GetImage();
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
        barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;

        vkQueueSubmit(device->GetGraphicsQueue().handle, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(device->GetGraphicsQueue().handle);

        vkFreeCommandBuffers(device->GetDevice(),
            renderer->GetCommandPool()->GetCommandPool(), 1, &cmd);

        m_irradianceReady = true;
    });

    return true;
}

VulkanTexture* CubeMap::GetPrefilteredCubemap() const
{
    return m_prefilteredCubemap.get();
}

bool CubeMap::IsPrefilteredReady() const
{
    return m_prefilteredReady;
}

void CubeMap::Unload()
{
}

VulkanTexture* CubeMap::GetBuffer() const
{
    return m_sampleMode == SampleMode::Irradiance && m_irradianceReady ? m_irradianceMap.get() : m_buffer.get();
}
