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
    
    /*
    auto resourceManager = Engine::Get()->GetResourceManager();
    auto computeShader = resourceManager->Load<Shader>(RESOURCE_PATH"shaders/PBR/mapPrefilter.shader");
    
    computeShader->EOnSentToGPU.Bind([computeShader, this, renderer]()
    {
        m_compute = computeShader->CreateDispatch(renderer);
        auto device = renderer->GetDevice();
        
        int baseResolution = 512;
        int mipLevels = static_cast<int>(std::floor(std::log2(baseResolution))) + 1;
        
        m_prefilteredCubemap = std::make_unique<VulkanTexture>();
        if (!m_prefilteredCubemap->CreateCubemapWithMips(baseResolution, mipLevels, 
                                                         device, 
                                                         renderer->GetCommandPool(), 
                                                         device->GetGraphicsQueue()))
        {
            PrintError("Failed to create prefiltered cubemap with mips");
            return;
        }
        
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = renderer->GetCommandPool()->GetCommandPool();
        allocInfo.commandBufferCount = 1;
        
        VulkanMaterial* mat = m_compute->GetMaterial();
        
        mat->SetTextureCube(0, 0, m_buffer->GetImageView(), m_buffer->GetSampler(), renderer);
        
        for (int mip = 0; mip < mipLevels; ++mip)
        {
            int mipResolution = baseResolution >> mip;
            
            VkCommandBuffer cmd;
            vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &cmd);
            
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &beginInfo);
            
            m_compute->GetMaterial()->GetPipeline()->Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
            
            VkImageView mipView = m_prefilteredCubemap->GetMipLevelView(mip);
            
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageView = mipView;
            imageInfo.sampler = VK_NULL_HANDLE;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = mat->GetDescriptorSet(0)->GetDescriptorSet(renderer->GetFrameIndex());
            write.dstBinding = 1;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device->GetDevice(), 1, &write, 0, nullptr);
            
            mat->BindForCompute(cmd, renderer->GetFrameIndex());
            
            for (int face = 0; face < 6; ++face)
            {
                struct PushConstants {
                    int mipLevel;
                    int mipCount;
                    int face;
                    int resolution;
                } pc;
                
                pc.mipLevel = mip;
                pc.mipCount = mipLevels;
                pc.face = face;
                pc.resolution = mipResolution;
                
                vkCmdPushConstants(cmd, mat->GetPipeline()->GetPipelineLayout(),
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);
                
                uint32_t groupsX = (mipResolution + 7) / 8;
                uint32_t groupsY = (mipResolution + 7) / 8;
                vkCmdDispatch(cmd, groupsX, groupsY, 1);
                
                if (face < 5 || mip < mipLevels - 1)
                {
                    VkMemoryBarrier memBarrier{};
                    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                    
                    vkCmdPipelineBarrier(cmd,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                         0, 1, &memBarrier, 0, nullptr, 0, nullptr);
                }
            }
            
            vkEndCommandBuffer(cmd);
            
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmd;
            
            vkQueueSubmit(device->GetGraphicsQueue().handle, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(device->GetGraphicsQueue().handle);
            
            vkFreeCommandBuffers(device->GetDevice(),
                                 renderer->GetCommandPool()->GetCommandPool(), 1, &cmd);
        }
        
        m_prefilteredReady = true;
    });
    */
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
