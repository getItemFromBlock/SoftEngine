#include "ParticleSystemComponent.h"

#include "Core/Engine.h"
#include "Render/Vulkan/VulkanRenderer.h"

void ParticleSystemComponent::OnCreate()
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();
    auto shader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/Compute/multiply.shader");

    shader->EOnSentToGPU.Bind([this, shader, renderer]()
    {
        m_compute = shader->CreateDispatch(renderer);
        const uint32_t elementCount = 1024;
        VkDeviceSize bufferSize = sizeof(float) * elementCount;
        
        auto device = Cast<VulkanRenderer>(renderer)->GetDevice();
        m_gpuBuffer = std::make_unique<VulkanBuffer>();
        m_stagingBuffer = std::make_unique<VulkanBuffer>();
        
        m_gpuBuffer->Initialize(device, bufferSize,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        m_stagingBuffer->Initialize(device, bufferSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    });
}

void ParticleSystemComponent::OnUpdate(float deltaTime)
{
    //TODO: Optimize this code
    if (!m_compute)
        return;

    VulkanMaterial* mat = Cast<VulkanMaterial>(m_compute->GetMaterial());
    auto renderer = Cast<VulkanRenderer>(Engine::Get()->GetRenderer());
    const uint32_t elementCount = 1024;

    // 1. Prepare input data
    std::vector<float> inputData(elementCount);
    for (uint32_t i = 0; i < elementCount; i++)
        inputData[i] = static_cast<float>(i);

    // 2. Copy input data to staging buffer
    m_stagingBuffer->CopyData(inputData.data(), sizeof(float) * elementCount);

    // 3. Get command buffer
    VkCommandBuffer commandBuffer = renderer->GetCommandBuffer();

    // 4. Copy from staging buffer to GPU buffer
    m_gpuBuffer->CopyFrom(commandBuffer, m_stagingBuffer.get(), sizeof(float) * elementCount);

    // 5. Barrier transfer -> compute
    VkBufferMemoryBarrier transferBarrier{};
    transferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    transferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    transferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    transferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transferBarrier.buffer = m_gpuBuffer->GetBuffer();
    transferBarrier.offset = 0;
    transferBarrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &transferBarrier, 0, nullptr
    );

    // 6. Bind buffer
    mat->SetStorageBuffer(0, 0, m_gpuBuffer->GetBuffer(), 0, sizeof(float) * elementCount, renderer);

    // 7. Dispatch compute
    uint32_t workGroupCount = (elementCount + 63) / 64;
    uint32_t frameIndex = renderer->GetFrameIndex();
    mat->BindForCompute(commandBuffer, frameIndex);
    mat->DispatchCompute(renderer, workGroupCount, 1, 1);

    // 8. Barrier compute -> transfer
    VkBufferMemoryBarrier computeBarrier{};
    computeBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    computeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    computeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    computeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    computeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    computeBarrier.buffer = m_gpuBuffer->GetBuffer();
    computeBarrier.offset = 0;
    computeBarrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &computeBarrier, 0, nullptr
    );

    // 9. Copy results back to staging buffer
    m_stagingBuffer->CopyFrom(commandBuffer, m_gpuBuffer.get(), sizeof(float) * elementCount);

    // 10. Submit command buffer and wait for execution
    vkEndCommandBuffer(commandBuffer);

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(renderer->GetDevice()->GetDevice(), &fci, nullptr, &fence);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(renderer->GetDevice()->GetGraphicsQueue().handle, 1, &submitInfo, fence);
    vkWaitForFences(renderer->GetDevice()->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(renderer->GetDevice()->GetDevice(), fence, nullptr);

    // 11. Readback results
    std::vector<float> outputData(elementCount);
    void* mappedData;
    vkMapMemory(renderer->GetDevice()->GetDevice(), m_stagingBuffer->GetBufferMemory(), 0, sizeof(float) * elementCount, 0, &mappedData);
    memcpy(outputData.data(), mappedData, sizeof(float) * elementCount);
    vkUnmapMemory(renderer->GetDevice()->GetDevice(), m_stagingBuffer->GetBufferMemory());

    for (size_t i = 0; i < elementCount; i++)
        std::cout << outputData[i] << " ";
    std::cout << std::endl;
}
