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

    GenerateIrradiance(renderer, 32);
    GeneratePrefiltered(renderer, 128, 7);
    GenerateBRDFLut(renderer, 512);
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
        imageInfo.imageView = allFacesView;
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

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = renderer->GetCommandPool()->GetCommandPool();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkImageMemoryBarrier initBarrier{};
        initBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        initBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        initBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        initBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initBarrier.image = m_irradianceMap->GetImage();
        initBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
        initBarrier.srcAccessMask = 0;
        initBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &initBarrier);

        mat->GetPipeline()->Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
        mat->BindForCompute(cmd, renderer->GetFrameIndex());

        uint32_t groups = (resolution + 7) / 8;

        for (int face = 0; face < 6; ++face)
        {
            struct PushConstants
            {
                int face;
                int resolution;
            } pc{face, static_cast<int>(resolution)};

            vkCmdPushConstants(cmd,
                               mat->GetPipeline()->GetPipelineLayout(),
                               VK_SHADER_STAGE_COMPUTE_BIT,
                               0, sizeof(PushConstants), &pc);

            vkCmdDispatch(cmd, groups, groups, 1);

            if (face < 5)
            {
                VkMemoryBarrier memBarrier{};
                memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                memBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     0, 1, &memBarrier, 0, nullptr, 0, nullptr);
            }
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_irradianceMap->GetImage();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        {
            std::scoped_lock lock(*device->GetGraphicsQueue().mutex);
            vkQueueSubmit(device->GetGraphicsQueue().handle, 1, &submitInfo, VK_NULL_HANDLE);
        }
        vkQueueWaitIdle(device->GetGraphicsQueue().handle);

        vkFreeCommandBuffers(device->GetDevice(),
                             renderer->GetCommandPool()->GetCommandPool(), 1, &cmd);

        m_irradianceReady = true;
    });

    return true;
}

bool CubeMap::GeneratePrefiltered(VulkanRenderer* renderer, uint32_t resolution, uint32_t mipLevels)
{
    if (!m_buffer)
    {
        PrintError("CubeMap: Cannot generate prefiltered map, source cubemap not on GPU yet");
        return false;
    }

    m_prefilteredCubemap = std::make_unique<VulkanTexture>();
    if (!m_prefilteredCubemap->CreateCubemapWithMips(
        static_cast<int>(resolution),
        static_cast<int>(mipLevels),
        renderer->GetDevice(),
        renderer->GetCommandPool(),
        renderer->GetDevice()->GetGraphicsQueue()))
    {
        PrintError("CubeMap: Failed to create prefiltered cubemap");
        return false;
    }

    auto resourceManager = Engine::Get()->GetResourceManager();
    SafePtr<Shader> prefilteredShader = resourceManager->Load<Shader>(
        RESOURCE_PATH"shaders/PrefilteredCompute/prefiltered.shader");

    prefilteredShader->EOnSentToGPU.Bind([this, renderer, resolution, mipLevels, prefilteredShader]()
    {
        m_prefilteredCompute = prefilteredShader->CreateDispatch(renderer);
        auto device = renderer->GetDevice();
        VulkanMaterial* mat = m_prefilteredCompute->GetMaterial();

        // Bind source environment cubemap (set 0, binding 0)
        mat->SetTextureCube(0, 0,
                            m_buffer->GetImageView(),
                            m_buffer->GetSampler(),
                            renderer);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = renderer->GetCommandPool()->GetCommandPool();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        mat->GetPipeline()->Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

        for (uint32_t mip = 0; mip < mipLevels; ++mip)
        {
            float roughness = static_cast<float>(mip) / static_cast<float>(mipLevels - 1);
            uint32_t mipResolution = std::max(1u, resolution >> mip);

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

            // record + submit for this mip
            allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = renderer->GetCommandPool()->GetCommandPool();
            allocInfo.commandBufferCount = 1;

            vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &cmd);

            beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &beginInfo);

            mat->GetPipeline()->Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
            mat->BindForCompute(cmd, renderer->GetFrameIndex());

            uint32_t groups = std::max(1u, (mipResolution + 7) / 8);

            for (int face = 0; face < 6; ++face)
            {
                struct PushConstants
                {
                    int face;
                    int resolution;
                    float roughness;
                } pc{face, static_cast<int>(mipResolution), roughness};

                vkCmdPushConstants(cmd,
                                   mat->GetPipeline()->GetPipelineLayout(),
                                   VK_SHADER_STAGE_COMPUTE_BIT,
                                   0, sizeof(PushConstants), &pc);

                vkCmdDispatch(cmd, groups, groups, 1);

                if (face < 5)
                {
                    VkMemoryBarrier memBarrier{};
                    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    memBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

                    vkCmdPipelineBarrier(cmd,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                         0, 1, &memBarrier, 0, nullptr, 0, nullptr);
                }
            }

            if (mip == mipLevels - 1)
            {
                VkImageMemoryBarrier finalBarrier{};
                finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                finalBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                finalBarrier.image = m_prefilteredCubemap->GetImage();
                finalBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 6};
                finalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &finalBarrier);
            }

            vkEndCommandBuffer(cmd);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmd;

            {
                std::scoped_lock lock(*device->GetGraphicsQueue().mutex);
                vkQueueSubmit(device->GetGraphicsQueue().handle, 1, &submitInfo, VK_NULL_HANDLE);
            }
            vkQueueWaitIdle(device->GetGraphicsQueue().handle);

            vkFreeCommandBuffers(device->GetDevice(),
                                 renderer->GetCommandPool()->GetCommandPool(), 1, &cmd);
        }

        m_prefilteredReady = true;
    });

    return true;
}

static std::map<uint32_t, std::vector<CubeMap*>> s_brdfWaiters;

bool CubeMap::GenerateBRDFLut(VulkanRenderer* renderer, uint32_t resolution)
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    std::string textureName = "BRDF_" + std::to_string(resolution);

    if (std::shared_ptr<Texture> tex = resourceManager->GetResource<Texture>(textureName))
    {
        m_brdfLutTexture = tex;
        m_brdfLutReady = true;
        return true;
    }

    std::filesystem::path cachePath = ResourceManager::GetCacheDir() / "brdf";
    std::filesystem::create_directories(cachePath);
    auto fileCachePath = cachePath / (textureName + ".cache");

    if (std::filesystem::exists(fileCachePath))
    {
        if (LoadBRDFLutFromCache(renderer, resolution, fileCachePath, textureName))
            return true;

        PrintError("CubeMap: Failed to load BRDF LUT from cache, recomputing");
    }

    if (s_brdfWaiters.contains(resolution))
    {
        s_brdfWaiters[resolution].push_back(this);
        return true;
    }

    s_brdfWaiters[resolution] = {};

    m_brdfLut = std::make_unique<VulkanTexture>();
    if (!m_brdfLut->Create(renderer->GetDevice(), resolution, resolution,
                           VK_FORMAT_R16G16_SFLOAT,
                           VK_IMAGE_USAGE_STORAGE_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    {
        PrintError("CubeMap: Failed to create BRDF LUT texture");
        s_brdfWaiters.erase(resolution);
        return false;
    }

    SafePtr<Shader> brdfShader = resourceManager->Load<Shader>(
        RESOURCE_PATH"shaders/BRDFLutCompute/brdf_lut.shader");

    brdfShader->EOnSentToGPU.Bind([this, renderer, resolution, brdfShader, textureName, fileCachePath]()
    {
        m_brdfLutCompute = brdfShader->CreateDispatch(renderer);
        DispatchBRDFLutCompute(renderer, resolution, fileCachePath, textureName);
    });

    return true;
}

bool CubeMap::LoadBRDFLutFromCache(VulkanRenderer* renderer, uint32_t resolution,
                                   const std::filesystem::path& fileCachePath,
                                   const std::string& textureName)
{
    const VkDeviceSize dataSize = resolution * resolution * 4;

    std::ifstream file(fileCachePath, std::ios::binary);
    if (!file)
        return false;

    std::vector<uint8_t> pixels(dataSize);
    file.read(reinterpret_cast<char*>(pixels.data()), dataSize);
    if (!file)
        return false;

    m_brdfLut = std::make_unique<VulkanTexture>();
    if (!m_brdfLut->Create(renderer->GetDevice(), resolution, resolution,
                           VK_FORMAT_R16G16_SFLOAT,
                           VK_IMAGE_USAGE_STORAGE_BIT |
                           VK_IMAGE_USAGE_SAMPLED_BIT |
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        return false;

    auto device = renderer->GetDevice();

    VulkanBuffer stagingBuffer;
    stagingBuffer.Initialize(device, dataSize,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* mapped;
    vkMapMemory(device->GetDevice(), stagingBuffer.GetBufferMemory(), 0, dataSize, 0, &mapped);
    memcpy(mapped, pixels.data(), dataSize);
    vkUnmapMemory(device->GetDevice(), stagingBuffer.GetBufferMemory());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = renderer->GetCommandPool()->GetCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = m_brdfLut->GetImage();
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {resolution, resolution, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer.GetBuffer(), m_brdfLut->GetImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier toReadOnly{};
    toReadOnly.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toReadOnly.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toReadOnly.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toReadOnly.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toReadOnly.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toReadOnly.image = m_brdfLut->GetImage();
    toReadOnly.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toReadOnly.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toReadOnly.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toReadOnly);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    {
        std::scoped_lock lock(*device->GetGraphicsQueue().mutex);
        vkQueueSubmit(device->GetGraphicsQueue().handle, 1, &submitInfo, VK_NULL_HANDLE);
    }
    vkQueueWaitIdle(device->GetGraphicsQueue().handle);
    vkFreeCommandBuffers(device->GetDevice(),
                         renderer->GetCommandPool()->GetCommandPool(), 1, &cmd);

    RegisterBRDFLutTexture(textureName, resolution);
    return true;
}

void CubeMap::DispatchBRDFLutCompute(VulkanRenderer* renderer, uint32_t resolution,
                                     const std::filesystem::path& fileCachePath,
                                     const std::string& textureName)
{
    auto device = renderer->GetDevice();
    VulkanMaterial* mat = m_brdfLutCompute->GetMaterial();

    // Transition
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = renderer->GetCommandPool()->GetCommandPool();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer transitionCmd;
        vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &transitionCmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(transitionCmd, &beginInfo);

        VkImageMemoryBarrier initBarrier{};
        initBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        initBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        initBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        initBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initBarrier.image = m_brdfLut->GetImage();
        initBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        initBarrier.srcAccessMask = 0;
        initBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(transitionCmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &initBarrier);

        vkEndCommandBuffer(transitionCmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &transitionCmd;

        {
            std::scoped_lock lock(*device->GetGraphicsQueue().mutex);
            vkQueueSubmit(device->GetGraphicsQueue().handle, 1, &submitInfo, VK_NULL_HANDLE);
        }
        vkQueueWaitIdle(device->GetGraphicsQueue().handle);
        vkFreeCommandBuffers(device->GetDevice(),
                             renderer->GetCommandPool()->GetCommandPool(), 1, &transitionCmd);
    }

    // Bind output LUT as storage image
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = m_brdfLut->GetImageView();
    imageInfo.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = mat->GetDescriptorSet(0)->GetDescriptorSet(renderer->GetFrameIndex());
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device->GetDevice(), 1, &write, 0, nullptr);

    // Record dispatch
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
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
    vkCmdDispatch(cmd, groups, groups, 1);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = m_brdfLut->GetImage();
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toTransfer.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    const VkDeviceSize dataSize = resolution * resolution * 4;
    VulkanBuffer stagingBuffer;
    stagingBuffer.Initialize(device, dataSize,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {resolution, resolution, 1};
    vkCmdCopyImageToBuffer(cmd, m_brdfLut->GetImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stagingBuffer.GetBuffer(), 1, &region);

    VkImageMemoryBarrier finalBarrier{};
    finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.image = m_brdfLut->GetImage();
    finalBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &finalBarrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    {
        std::scoped_lock lock(*device->GetGraphicsQueue().mutex);
        vkQueueSubmit(device->GetGraphicsQueue().handle, 1, &submitInfo, VK_NULL_HANDLE);
    }
    vkQueueWaitIdle(device->GetGraphicsQueue().handle);
    vkFreeCommandBuffers(device->GetDevice(),
                         renderer->GetCommandPool()->GetCommandPool(), 1, &cmd);

    // Write to file, so no need to compute every launch
    void* mapped;
    vkMapMemory(device->GetDevice(), stagingBuffer.GetBufferMemory(), 0, dataSize, 0, &mapped);
    std::ofstream file(fileCachePath, std::ios::binary | std::ios::trunc);
    if (file)
    {
        file.write(static_cast<const char*>(mapped), dataSize);
    }
    else
    {
        PrintError("CubeMap: Failed to write BRDF LUT cache to %s",
               fileCachePath.generic_string().c_str());
    }
    vkUnmapMemory(device->GetDevice(), stagingBuffer.GetBufferMemory());

    RegisterBRDFLutTexture(textureName, resolution);
}

void CubeMap::RegisterBRDFLutTexture(const std::string& textureName, uint32_t resolution)
{
    m_brdfLutTexture = std::make_shared<Texture>(textureName);
    m_brdfLutTexture->CreateFromBuffer(
        GBufferAttachment{
            .image = m_brdfLut->GetImage(),
            .memory = m_brdfLut->GetImageMemory(),
            .imageView = m_brdfLut->GetImageView(),
            .format = m_brdfLut->GetFormat()
        },
        m_brdfLut->GetSampler(),
        resolution, resolution);
    m_brdfLutTexture->SetLoaded();
    m_brdfLutTexture->SetSentToGPU();

    ResourceManager* rm = Engine::Get()->GetResourceManager();
    rm->AddResource(m_brdfLutTexture);
    m_brdfLutReady = true;

    auto it = s_brdfWaiters.find(resolution);
    if (it != s_brdfWaiters.end())
    {
        for (CubeMap* waiter : it->second)
        {
            waiter->m_brdfLutTexture = m_brdfLutTexture;
            waiter->m_brdfLutReady = true;
        }
        s_brdfWaiters.erase(it);
    }
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
    switch (m_sampleMode)
    {
    case SampleMode::Irradiance:
        return m_irradianceReady ? m_irradianceMap.get() : m_buffer.get();
    case SampleMode::Prefilter:
        return m_prefilteredReady ? m_prefilteredCubemap.get() : m_buffer.get();
    default:
        return m_buffer.get();
    }
}
