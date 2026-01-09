#pragma once

#include "VulkanPipeline.h"
#include "VulkanMaterial.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <ranges>
#include <stdexcept>

#include "VulkanUtils.h"
#include "Resource/Mesh.h"
#include "Resource/Shader.h"
#include "VulkanShaderBuffer.h"
#include "Debug/Log.h"

#include <shaderc/shaderc.hpp>

#include "Resource/ComputeShader.h"
#include "Resource/FragmentShader.h"
#include "Resource/VertexShader.h"
#include "Utils/SPVReflection.h"

std::vector<char> CompileGLSLToSPV(const std::string& source, shaderc_shader_kind kind) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(source, kind, "shader.glsl", options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        PrintError("Shader compilation failed: %s", module.GetErrorMessage().c_str());
        return {};
    }
    
    std::vector<uint32_t> spirv(module.begin(), module.end());
    
    const char* begin = reinterpret_cast<const char*>(spirv.data());
    const char* end = begin + spirv.size() * sizeof(uint32_t);
    return std::vector<char>(begin, end);
}

static VkDescriptorType ConvertType(UniformType type)
{
    switch (type) {
    case UniformType::NestedStruct:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case UniformType::StorageBuffer: 
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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
    case ShaderType::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default: 
        break;
    }
    PrintError("Invalid shader type");
    return VK_SHADER_STAGE_ALL;
}

VulkanPipeline::~VulkanPipeline()
{
    Cleanup();
}

bool VulkanPipeline::Initialize(VulkanDevice* device, VkExtent2D extent,
                                uint32_t maxFramesInFlight, const Shader* shader,
                                VkFormat colorFormat, VkFormat depthFormat)
{
    auto uniforms = shader->GetUniforms();
    auto pushConstants = shader->GetPushConstants();
    
    auto fragmentShader = shader->GetFragmentShader();
    auto vertexShader = shader->GetVertexShader();
    auto computeShader = shader->GetComputeShader();

    m_device = device;
    m_maxFramesInFlight = maxFramesInFlight;

    try
    {
        std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> layoutBindings;
        for (const auto& uniform : uniforms | std::views::values)
        {
            VkDescriptorSetLayoutBinding layoutBinding{};
            layoutBinding.binding = uniform.binding;
            layoutBinding.descriptorCount = 1; 
            layoutBinding.descriptorType = ConvertType(uniform.type);
            layoutBinding.pImmutableSamplers = nullptr;
            
            layoutBinding.stageFlags = ConvertType(uniform.shaderType);
    
            layoutBindings[uniform.set].push_back(layoutBinding);
            m_descriptorTypeCounts[layoutBinding.descriptorType] += 1;
            m_uniformsBySet[uniform.set].push_back(uniform);

            if (layoutBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                UBOBinding key = {uniform.set, uniform.binding};
                m_uniformBufferSizes[key] = uniform.size;
            }
        }

        for (const auto& [set, bindings] : layoutBindings)
        {
            auto layout = std::make_unique<VulkanDescriptorSetLayout>(m_device->GetDevice());
            layout->Create(m_device->GetDevice(), bindings);
            m_descriptorSetLayouts.push_back(std::move(layout));
        }

        std::vector<VkPushConstantRange> ranges;
        for (auto& [shaderType, pc] : pushConstants)
        {
            VkShaderStageFlags stageFlags = 0;
            if (shaderType == ShaderType::Vertex)   stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            if (shaderType == ShaderType::Fragment) stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            if (shaderType == ShaderType::Compute)  stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
                
            ranges.push_back({ .stageFlags = stageFlags, .offset = pc.offset, .size = pc.size });
        }

        std::vector<VkDescriptorSetLayout> layouts;
        for (const auto& layout : m_descriptorSetLayouts) layouts.push_back(layout->GetLayout());

        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        layoutInfo.pSetLayouts = layouts.data();
        layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
        layoutInfo.pPushConstantRanges = ranges.data();

        if (vkCreatePipelineLayout(m_device->GetDevice(), &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout");

        if (computeShader) 
        {
            return InitializeComputePipeline(computeShader);
        }
        else 
        {
            return InitializeGraphicsPipeline(vertexShader, fragmentShader, colorFormat, depthFormat);
        }
    }
    catch (const std::exception& e) {
        Cleanup();
        std::cerr << "Pipeline init failed: " << e.what() << std::endl;
        return false;
    }
}

bool VulkanPipeline::InitializeGraphicsPipeline(const VertexShader* vertexShader, 
                                               const FragmentShader* fragmentShader,
                                               VkFormat colorFormat,
                                               VkFormat depthFormat)
{
    VkPipelineShaderStageCreateInfo vertStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertexShader->GetBuffer()->GetModule();
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragmentShader->GetBuffer()->GetModule();
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

    std::string vertexContent = vertexShader->GetContent();
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    std::vector<VkVertexInputBindingDescription> bindingDescription;
    SPV::ReflectVertexInputs(vertexContent, attributeDescriptions, bindingDescription);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescription.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescription.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
    dynamicState.pDynamicStates = dynamicStates;
    
    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;
    pipelineRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.pNext = &pipelineRenderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(m_device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
    return result == VK_SUCCESS;
}


bool VulkanPipeline::InitializeComputePipeline(const ComputeShader* computeShader)
{
    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    
    // Shader Stage
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = computeShader->GetBuffer()->GetModule();
    pipelineInfo.stage.pName = "main";
    
    pipelineInfo.layout = m_pipelineLayout;

    VkResult result = vkCreateComputePipelines(m_device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
    return result == VK_SUCCESS;
}

void VulkanPipeline::Cleanup()
{
    if (!m_device)
        return;
        
    m_descriptorSetLayouts.clear();
    
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

void VulkanPipeline::Bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint)
{
    if (m_pipeline != VK_NULL_HANDLE)
        vkCmdBindPipeline(commandBuffer, bindPoint, m_pipeline);
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
