#pragma once
#include <vulkan/vulkan.h>
#include "spirv_reflect.h"
#include <vector>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <algorithm>

#include "Resource/Shader.h"

namespace SPV
{
    VkFormat SpvFormatToVkFormat(SpvReflectFormat f);
    uint32_t VkFormatSize(VkFormat fmt);

    void ReflectVertexInputs(const std::string& spirv, std::vector<VkVertexInputAttributeDescription>& outAttributes, std::vector<VkVertexInputBindingDescription>& outBindings);
    void ReflectDescriptorBindings(const std::string& spirv, std::vector<VkDescriptorSetLayoutBinding>& outBindings, VkShaderStageFlags& outStageFlags);

    uint32_t CalculateStorageBufferSize(const SpvReflectDescriptorBinding* binding);
    size_t CalculateStorageBufferSizeWithCount(const SpvReflectDescriptorBinding* binding,
                                               uint32_t runtimeArrayElementCount = 0);
    void ParseBlockVariable(const SpvReflectBlockVariable* var, UniformMember& out);

    Uniforms SpirvReflectUniforms(const std::string& spirv);

    std::optional<PushConstant> SpirvReflectPushConstants(const std::string& spirv);
}
