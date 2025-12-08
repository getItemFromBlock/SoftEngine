#pragma once
#ifdef RENDER_API_VULKAN

#include "VulkanPipeline.h"

#include <filesystem>
#include <vector>
#include <array>
#include <iostream>
#include <fstream>
#include <stdexcept>

#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include "Resource/Mesh.h"
#include "Resource/Shader.h"

#include <shaderc/shaderc.hpp>

#include "VulkanShaderBuffer.h"
#include "VulkanUniformBuffer.h"
#include "Debug/Log.h"

std::vector<char> CompileGLSLToSPV(const std::string& source, shaderc_shader_kind kind) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(source, kind, "shader.glsl", options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        PrintError("Shader compilation failed: %s", module.GetErrorMessage().c_str());;
        return {};
    }
    
    std::vector<uint32_t> spirv(module.begin(), module.end());
    
    const char* begin = reinterpret_cast<const char*>(spirv.data());
    const char* end = begin + spirv.size() * sizeof(uint32_t);
    return std::vector<char>(begin, end);
}

#include "spirv_reflect.h"

int SpirvReflectExample(const void* spirv_code, size_t spirv_nbytes)
{
    // Generate reflection data for a shader
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirv_nbytes, spirv_code, &module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // Enumerate and extract shader's input variables
    uint32_t var_count = 0;
    result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    SpvReflectInterfaceVariable** input_vars =
      (SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
    result = spvReflectEnumerateInputVariables(&module, &var_count, input_vars);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // Output variables, descriptor bindings, descriptor sets, and push constants
    // can be enumerated and extracted using a similar mechanism.

    // Destroy the reflection data when no longer required.
    spvReflectDestroyShaderModule(&module);
    
    return 0;
}

VulkanPipeline::~VulkanPipeline()
{
    Cleanup();
}

bool VulkanPipeline::Initialize(VulkanDevice* device,
                                VkRenderPass renderPass,
                                VkExtent2D extent,
                                const std::string& vertShaderPath,
                                const std::string& fragShaderPath,
                                const std::vector<VkDescriptorSetLayoutBinding>& bindings /*= {}*/,
                                VkSampleCountFlagBits msaaSamples /*= VK_SAMPLE_COUNT_1_BIT*/
                                , bool enableDepth /*= true*/
                                , bool compiled /*= false*/)
{
    if (!device || renderPass == VK_NULL_HANDLE)
        return false;

    m_device = device;
    
    std::vector<char> vertShaderCode;
    std::vector<char> fragShaderCode;
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;

    m_descriptorSetLayouts.push_back(std::make_unique<VulkanDescriptorSetLayout>(device->GetDevice()));
    m_descriptorSetLayouts.back()->Create(device->GetDevice(), bindings);

    try
    {
        if (compiled)
        {
            vertShaderCode = ReadFileBin(vertShaderPath);
            fragShaderCode = ReadFileBin(fragShaderPath);
        }
        else
        {
        
            std::string vert = ReadFile(vertShaderPath);
            std::string frag = ReadFile(fragShaderPath);
        
            vertShaderCode = CompileGLSLToSPV(vert, shaderc_vertex_shader);
            fragShaderCode = CompileGLSLToSPV(frag, shaderc_fragment_shader);
        }
        
        SpirvReflectExample(vertShaderCode.data(), vertShaderCode.size());
        SpirvReflectExample(fragShaderCode.data(), fragShaderCode.size());

        if (vertShaderCode.empty() || fragShaderCode.empty())
            throw std::runtime_error("Shader file is empty");

        vertShaderModule = CreateShaderModule(vertShaderCode);
        fragShaderModule = CreateShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto bindingDescription = VulkanUtils::To(Vertex::GetBindingDescription());
        
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
        std::array<RHIVertexInputAttributeDescription, 4> descriptions = Vertex::GetAttributeDescriptions();
        for (size_t i = 0; i < attributeDescriptions.size(); ++i)
            attributeDescriptions[i] = VulkanUtils::To(descriptions[i]);

        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = { .x=0, .y= 0 };
        scissor.extent = extent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        viewportState.pViewports = nullptr;
        viewportState.pScissors = nullptr;

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
        dynamicState.pDynamicStates = dynamicStates;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = msaaSamples;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        if (enableDepth)
        {
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        auto vkDescriptorSetLayout = m_descriptorSetLayouts[0]->GetLayout();
        pipelineLayoutInfo.pSetLayouts = &vkDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VkResult result = vkCreatePipelineLayout(m_device->GetDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout. VkResult: " + std::to_string(result));

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = enableDepth ? &depthStencil : nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        result = vkCreateGraphicsPipelines(m_device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create graphics pipeline. VkResult: " + std::to_string(result));

        vkDestroyShaderModule(m_device->GetDevice(), vertShaderModule, nullptr);
        vertShaderModule = VK_NULL_HANDLE;
        vkDestroyShaderModule(m_device->GetDevice(), fragShaderModule, nullptr);
        fragShaderModule = VK_NULL_HANDLE;

        return true;
    }
    catch (const std::exception& e)
    {
        if (vertShaderModule != VK_NULL_HANDLE)
            vkDestroyShaderModule(m_device->GetDevice(), vertShaderModule, nullptr);
        if (fragShaderModule != VK_NULL_HANDLE)
            vkDestroyShaderModule(m_device->GetDevice(), fragShaderModule, nullptr);
        Cleanup();
        std::cerr << "VulkanPipeline initialization failed: " << e.what() << std::endl;
        return false;
    }
}

static VkDescriptorType ConvertType(UniformType type)
{
    switch (type) {
    case UniformType::NestedStruct:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case UniformType::Sampler2D:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case UniformType::SamplerCube:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    default:
        break;
    }
    PrintError("Invalid uniform type");
    return VK_DESCRIPTOR_TYPE_SAMPLER;
}

static VkShaderStageFlags ConvertType(ShaderType type)
{
    switch (type)
    {
    case ShaderType::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderType::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    default: 
        break;
    }
    PrintError("Invalid shader type");
    return VK_SHADER_STAGE_ALL;
}

bool VulkanPipeline::Initialize(VulkanDevice* device, VkRenderPass renderPass, VkExtent2D extent,
                                const std::vector<Uniform>& uniforms, const VertexShader* vertexShader, const FragmentShader* fragShader,
                                uint32_t MAX_FRAMES_IN_FLIGHT, Texture* defaultTexture)
{
    m_device = device;
    try
    {
        // --- Descriptor Set Layouts and Pool Sizing ---

        std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> layoutBindings;
        std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeCounts;
        uint32_t maxUniformBufferSize = 0; // Initialize tracking for the largest required UBO size

        for (const auto& uniform : uniforms)
        {
            VkDescriptorSetLayoutBinding layoutBinding{};
            layoutBinding.binding = uniform.binding;
            layoutBinding.descriptorCount = 1; 
            layoutBinding.descriptorType = ConvertType(uniform.type);
            layoutBinding.pImmutableSamplers = nullptr;
            layoutBinding.stageFlags = ConvertType(uniform.shaderType);
            
            layoutBindings[uniform.set].push_back(layoutBinding);

            descriptorTypeCounts[layoutBinding.descriptorType] += 1;

            // Track the maximum size needed for any Uniform Buffer
            if (layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                maxUniformBufferSize = std::max(maxUniformBufferSize, uniform.size);
            }
        }

        for (const auto& [set, bindings] : layoutBindings)
        {
            std::unique_ptr<VulkanDescriptorSetLayout> vulkanDescriptorSetLayout = std::make_unique<
                VulkanDescriptorSetLayout>(m_device->GetDevice());
            m_descriptorSetLayouts.push_back(std::move(vulkanDescriptorSetLayout));
            m_descriptorSetLayouts.back()->Create(m_device->GetDevice(), bindings);
        }
        
        // --- Shader Stages ---

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = dynamic_cast<VulkanShaderBuffer*>(vertexShader->GetBuffer())->GetModule();
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = dynamic_cast<VulkanShaderBuffer*>(fragShader->GetBuffer())->GetModule();
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // --- Vertex Input ---

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        auto bindingDescription = VulkanUtils::To(Vertex::GetBindingDescription());
        
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
        std::array<RHIVertexInputAttributeDescription, 4> descriptions = Vertex::GetAttributeDescriptions();
        for (size_t i = 0; i < attributeDescriptions.size(); ++i)
            attributeDescriptions[i] = VulkanUtils::To(descriptions[i]);

        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // --- Input Assembly ---

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // --- Viewport/Scissor (Dynamic) and Rasterization/Multisampling/Color Blend ---

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        viewportState.pViewports = nullptr;
        viewportState.pScissors = nullptr;

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
        dynamicState.pDynamicStates = dynamicStates;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        // --- Depth Stencil ---
        
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        constexpr bool enableDepth = true;
        if (enableDepth)
        {
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;
        }

        // --- Pipeline Layout and Creation ---

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        
        std::vector<VkDescriptorSetLayout> layouts;
        layouts.reserve(m_descriptorSetLayouts.size());
        for (const auto& layout : m_descriptorSetLayouts)
        {
            layouts.push_back(layout->GetLayout());
        }
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VkResult result = vkCreatePipelineLayout(m_device->GetDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout. VkResult: " + std::to_string(result));

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = enableDepth ? &depthStencil : nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        result = vkCreateGraphicsPipelines(m_device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create graphics pipeline. VkResult: " + std::to_string(result));
        
        // --- UNIFORM BUFFER INITIALIZATION (Max Size) ---
        
        if (maxUniformBufferSize > 0)
        {
            m_uniformBuffer = std::make_unique<VulkanUniformBuffer>();
            // Use the calculated maximum size for all uniform buffers
            if (!m_uniformBuffer->Initialize(m_device, maxUniformBufferSize, MAX_FRAMES_IN_FLIGHT)) 
            {
                PrintError("Failed to initialize uniform buffer!");
                return false;
            }
            m_uniformBuffer->MapAll();
        }
        
        // --- Descriptor Pool Creation ---
        
        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(descriptorTypeCounts.size());
        for (const auto& [type, count] : descriptorTypeCounts)
        {
            poolSizes.push_back(
            {
                .type = type, 
                .descriptorCount = count * MAX_FRAMES_IN_FLIGHT
            }); 
        }

        m_descriptorPool = std::make_unique<VulkanDescriptorPool>();
        
        if (!m_descriptorPool->Initialize(m_device, poolSizes, MAX_FRAMES_IN_FLIGHT * m_descriptorSetLayouts.size()))
        {
            PrintError("Failed to initialize descriptor pool!");
            return false;
        }
        
        // --- Descriptor Set Allocation/Initialization ---
        for (const auto& m_descriptorSetLayout : m_descriptorSetLayouts)
        {
            std::unique_ptr<VulkanDescriptorSet> descriptorSet = std::make_unique<VulkanDescriptorSet>();
            descriptorSet->Initialize(m_device, m_descriptorPool->GetPool(), m_descriptorSetLayout->GetLayout(), MAX_FRAMES_IN_FLIGHT);
            m_descriptorSets.push_back(std::move(descriptorSet));
        }
        
        for (uint32_t j = 0; j < m_descriptorSets.size(); j++)
        {
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                m_descriptorSets[j]->UpdateDescriptorSets(i, j, uniforms, m_uniformBuffer.get(), defaultTexture);
            }
        }
        
        return true;
    }
    catch (const std::exception& e)
    {
        Cleanup();
        std::cerr << "VulkanPipeline initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void VulkanPipeline::Cleanup()
{
    if (!m_device)
        return;
    // if (m_descriptorSetLayout != VK_NULL_HANDLE)
    // {
    //     vkDestroyDescriptorSetLayout(m_device->GetDevice(), m_descriptorSetLayout, nullptr);
    //     m_descriptorSetLayout = VK_NULL_HANDLE;
    // }
    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device->GetDevice(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device->GetDevice(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

std::vector<VulkanDescriptorSetLayout*> VulkanPipeline::GetDescriptorSetLayouts() const
{
    std::vector<VulkanDescriptorSetLayout*> layouts;
    for (auto& layout : m_descriptorSetLayouts)
        layouts.push_back(layout.get());
    return layouts;
}

std::vector<VulkanDescriptorSet*> VulkanPipeline::GetDescriptorSets() const
{
    std::vector<VulkanDescriptorSet*> sets;
    for (auto& set : m_descriptorSets)
        sets.push_back(set.get());
    return sets;
}

void VulkanPipeline::Bind(VkCommandBuffer commandBuffer)
{
    if (m_pipeline != VK_NULL_HANDLE)
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

VkShaderModule VulkanPipeline::CreateShaderModule(const std::vector<char>& code) const
{
    if (code.empty() || (code.size() % 4) != 0)
        throw std::runtime_error("Invalid SPIR-V code size");

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(m_device->GetDevice(), &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module. VkResult: " + std::to_string(result));

    return shaderModule;
}

std::vector<char> VulkanPipeline::ReadFileBin(const std::string& filename)
{
    namespace fs = std::filesystem;

    if (!fs::exists(filename))
        throw std::runtime_error("File does not exist: " + filename);

    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open file: " + filename);

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0)
        return {};

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();
    return buffer;
}

std::string VulkanPipeline::ReadFile(const std::string& filename)
{
    std::ifstream t(filename);
    std::stringstream buffer;
    buffer << t.rdbuf();

    return buffer.str();
}

#endif
