#include "SPVReflection.h"

#include <map>
#include <set>

#include "Debug/Log.h"

VkFormat SPV::SpvFormatToVkFormat(SpvReflectFormat f)
{
    switch (f)
    {
    case SPV_REFLECT_FORMAT_R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
    case SPV_REFLECT_FORMAT_R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
    case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
    case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case SPV_REFLECT_FORMAT_R16G16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT;
    case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
    default: return VK_FORMAT_UNDEFINED;
    }
}

uint32_t SPV::VkFormatSize(VkFormat fmt)
{
    switch (fmt)
    {
    case VK_FORMAT_R32_SFLOAT: return 4;
    case VK_FORMAT_R32G32_SFLOAT: return 8;
    case VK_FORMAT_R32G32B32_SFLOAT: return 12;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return 16;
    case VK_FORMAT_R8G8B8A8_UNORM: return 4;
    case VK_FORMAT_R8G8B8A8_UINT: return 4;
    case VK_FORMAT_R16G16_SFLOAT: return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return 8;
    default: return 0;
    }
}

void SPV::ReflectVertexInputs(const std::string& spirv,
                             std::vector<VkVertexInputAttributeDescription>& outAttributes,
                             std::vector<VkVertexInputBindingDescription>& outBindings)
{
    const uint32_t* spirv_words = reinterpret_cast<const uint32_t*>(spirv.data());
    size_t word_count = spirv.size() / sizeof(uint32_t);

    SpvReflectShaderModule module;
    if (spvReflectCreateShaderModule(word_count * 4, spirv_words, &module) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("spvReflectCreateShaderModule failed");

    uint32_t var_count = 0;
    if (spvReflectEnumerateInputVariables(&module, &var_count, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("spvReflectEnumerateInputVariables failed");

    std::vector<SpvReflectInterfaceVariable*> vars(var_count);
    if (spvReflectEnumerateInputVariables(&module, &var_count, vars.data()) != SPV_REFLECT_RESULT_SUCCESS)
        throw std::runtime_error("spvReflectEnumerateInputVariables failed (2)");

    auto toLower = [](const char* s) {
        std::string r;
        if (!s) return r;
        for (; *s; ++s) r.push_back(static_cast<char>(std::tolower(*s)));
        return r;
    };

    std::map<uint32_t, std::vector<VkVertexInputAttributeDescription>> perBindingAttrs;
    std::map<uint32_t, uint32_t> bindingStrides;
    std::set<uint32_t> usedBindings;

    for (SpvReflectInterfaceVariable* var : vars)
    {
        if (var->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
            continue;

        VkFormat baseFormat = SpvFormatToVkFormat(var->format);
        if (baseFormat == VK_FORMAT_UNDEFINED)
            continue;

        bool isMatrix = (var->type_description->type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) != 0;

        uint32_t columns = 1;
        uint32_t rows = 1;
        if (isMatrix) {
            columns = var->type_description->traits.numeric.matrix.column_count;
            rows    = var->type_description->traits.numeric.matrix.row_count;
        }

        std::string nameLower = toLower(var->name);
        bool isInstance = false;
        if (!nameLower.empty() && nameLower.find("instance") != std::string::npos)
            isInstance = true;
        if (!isInstance && isMatrix && var->location >= 4)
            isInstance = true;

        uint32_t binding = isInstance ? 1u : 0u;
        usedBindings.insert(binding);

        VkFormat columnFormat = baseFormat;
        if (columns > 1) {
            if (rows == 4)      columnFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            else if (rows == 3) columnFormat = VK_FORMAT_R32G32B32_SFLOAT;
        }

        for (uint32_t c = 0; c < columns; ++c)
        {
            VkVertexInputAttributeDescription attr{};
            attr.location = var->location + c;
            attr.binding  = binding;
            attr.format   = columnFormat;
            attr.offset   = 0;
            perBindingAttrs[binding].push_back(attr);
        }
    }

    outAttributes.clear();
    outBindings.clear();

    for (uint32_t binding : usedBindings)
    {
        auto& attrs = perBindingAttrs[binding];

        std::sort(attrs.begin(), attrs.end(),
                  [](const VkVertexInputAttributeDescription& a,
                     const VkVertexInputAttributeDescription& b) {
                      return a.location < b.location;
                  });

        uint32_t offset = 0;
        for (auto& attr : attrs)
        {
            attr.offset = offset;
            offset += VkFormatSize(attr.format);
            outAttributes.push_back(attr);
        }

        bindingStrides[binding] = offset;

        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding   = binding;
        bindingDesc.stride    = offset;
        bindingDesc.inputRate = (binding == 1)
                              ? VK_VERTEX_INPUT_RATE_INSTANCE
                              : VK_VERTEX_INPUT_RATE_VERTEX;
        outBindings.push_back(bindingDesc);
    }

    std::sort(outAttributes.begin(), outAttributes.end(),
              [](const VkVertexInputAttributeDescription& a,
                 const VkVertexInputAttributeDescription& b) {
                  if (a.binding != b.binding) return a.binding < b.binding;
                  return a.location < b.location;
              });

    spvReflectDestroyShaderModule(&module);
}


void SPV::ReflectDescriptorBindings(const std::string& spirv, std::vector<VkDescriptorSetLayoutBinding>& outBindings,
    VkShaderStageFlags& outStageFlags)
{
    const uint32_t* spirv_words = reinterpret_cast<const uint32_t*>(spirv.data());
    size_t word_count = spirv.size() / sizeof(uint32_t);

    SpvReflectShaderModule module;
    SpvReflectResult res = spvReflectCreateShaderModule(word_count * 4, spirv_words, &module);
    if (res != SPV_REFLECT_RESULT_SUCCESS) throw std::runtime_error("Failed to create SPIRV-Reflect module");

    outStageFlags = 0;
    switch (module.shader_stage)
    {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT: outStageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT: outStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT: outStageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT: outStageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT: outStageFlags =
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: outStageFlags =
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    default: outStageFlags = 0;
        break;
    }

    uint32_t desc_count = 0;
    res = spvReflectEnumerateDescriptorBindings(&module, &desc_count, NULL);
    if (res != SPV_REFLECT_RESULT_SUCCESS)
    {
        spvReflectDestroyShaderModule(&module);
        throw std::runtime_error("Failed to enumerate descriptor bindings");
    }

    std::vector<SpvReflectDescriptorBinding*> descs(desc_count);
    res = spvReflectEnumerateDescriptorBindings(&module, &desc_count, descs.data());
    if (res != SPV_REFLECT_RESULT_SUCCESS)
    {
        spvReflectDestroyShaderModule(&module);
        throw std::runtime_error("Failed to enumerate descriptor bindings (2)");
    }

    outBindings.clear();
    for (uint32_t i = 0; i < desc_count; ++i)
    {
        SpvReflectDescriptorBinding* d = descs[i];
        VkDescriptorType dtype = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        switch (d->descriptor_type)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: dtype = VK_DESCRIPTOR_TYPE_SAMPLER;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: dtype = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: dtype = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: dtype = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: dtype = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: dtype = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: dtype = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: dtype = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: dtype = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: dtype = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: dtype = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            break;
        default: dtype = VK_DESCRIPTOR_TYPE_MAX_ENUM;
            break;
        }
        if (dtype == VK_DESCRIPTOR_TYPE_MAX_ENUM) continue;

        uint32_t array_count = 1;
        if (d->array.dims_count > 0)
        {
            array_count = 1;
            for (uint32_t k = 0; k < d->array.dims_count; ++k) array_count *= d->array.dims[k];
        }

        VkDescriptorSetLayoutBinding bind{};
        bind.binding = d->binding;
        bind.descriptorType = dtype;
        bind.descriptorCount = array_count;
        bind.pImmutableSamplers = nullptr;
        bind.stageFlags = outStageFlags;
        outBindings.push_back(bind);
    }

    spvReflectDestroyShaderModule(&module);
}

size_t SPV::CalculateStorageBufferSize(const SpvReflectDescriptorBinding* binding)
{
    // If the block has a known size, use it
    if (binding->block.size > 0)
    {
        return binding->block.size;
    }

    // For runtime-sized arrays (common in SSBOs), calculate size from members
    size_t calculatedSize = 0;
    bool hasRuntimeArray = false;

    if (binding->block.member_count > 0 && binding->block.members)
    {
        for (uint32_t m = 0; m < binding->block.member_count; ++m)
        {
            const SpvReflectBlockVariable* member = &binding->block.members[m];

            // Check if this is a runtime array (array with no explicit size)
            if (member->array.dims_count > 0 && member->array.dims[member->array.dims_count - 1] == 0)
            {
                hasRuntimeArray = true;
                // For runtime arrays, we need to handle this differently
                // The size should be set dynamically when creating the buffer
                // Use VK_WHOLE_SIZE or a reasonable default
                calculatedSize = member->offset; // Size up to the runtime array
                break;
            }

            // Calculate the end offset of this member
            size_t memberEnd = member->offset + member->size;
            if (memberEnd > calculatedSize)
            {
                calculatedSize = memberEnd;
            }
        }
    }

    // If we have a runtime array, return 0 to indicate dynamic sizing needed
    if (hasRuntimeArray)
    {
        return 0; // Caller must set size manually
    }

    // Otherwise return the calculated size, or a default minimum
    return calculatedSize > 0 ? calculatedSize : 256; // 256 bytes minimum default
}

size_t SPV::CalculateStorageBufferSizeWithCount(const SpvReflectDescriptorBinding* binding,
    uint32_t runtimeArrayElementCount)
{
    if (binding->block.size > 0 && runtimeArrayElementCount == 0)
    {
        return binding->block.size;
    }

    size_t totalSize = 0;

    if (binding->block.member_count > 0 && binding->block.members)
    {
        for (uint32_t m = 0; m < binding->block.member_count; ++m)
        {
            const SpvReflectBlockVariable* member = &binding->block.members[m];

            // Check for runtime array
            if (member->array.dims_count > 0 &&
                member->array.dims[member->array.dims_count - 1] == 0)
            {
                // Calculate size of elements before runtime array
                totalSize = member->offset;

                if (runtimeArrayElementCount > 0)
                {
                    // Add size for runtime array elements
                    size_t elementSize = member->size;

                    // Account for array stride if available
                    if (member->array.stride > 0)
                    {
                        elementSize = member->array.stride;
                    }

                    totalSize += elementSize * runtimeArrayElementCount;
                }

                break;
            }

            // For fixed-size members
            size_t memberEnd = member->offset + member->size;
            if (memberEnd > totalSize)
            {
                totalSize = memberEnd;
            }
        }
    }

    return totalSize > 0 ? totalSize : 256;
}

void SPV::ParseBlockVariable(const SpvReflectBlockVariable* var, UniformMember& out)
{
    if (!var) return;

    if (var->name && var->name[0]) out.name = var->name;
    else if (var->type_description && var->type_description->type_name)
        out.name = var->type_description->type_name;
    else
        out.name = "";

    if (var->type_description && var->type_description->type_name)
        out.typeName = var->type_description->type_name;
    else
        out.typeName = "";

    out.offset = var->offset;
    out.size = var->size;

    if (var->type_description)
    {
        const SpvReflectTypeDescription* type_desc = var->type_description;

        if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT)
        {
            out.type = UniformType::NestedStruct;
        }
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX)
        {
            if (type_desc->traits.numeric.matrix.column_count == 2 &&
                type_desc->traits.numeric.matrix.row_count == 2)
            {
                out.type = UniformType::Mat2;
            }
            else if (type_desc->traits.numeric.matrix.column_count == 3 &&
                type_desc->traits.numeric.matrix.row_count == 3)
            {
                out.type = UniformType::Mat3;
            }
            else if (type_desc->traits.numeric.matrix.column_count == 4 &&
                type_desc->traits.numeric.matrix.row_count == 4)
            {
                out.type = UniformType::Mat4;
            }
            else
            {
                out.type = UniformType::Unknown;
            }
        }
        // Check for vector types
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR)
        {
            uint32_t component_count = type_desc->traits.numeric.vector.component_count;

            if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
            {
                if (component_count == 2) out.type = UniformType::Vec2;
                else if (component_count == 3) out.type = UniformType::Vec3;
                else if (component_count == 4) out.type = UniformType::Vec4;
                else out.type = UniformType::Unknown;
            }
            else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_INT)
            {
                if (component_count == 2) out.type = UniformType::IVec2;
                else if (component_count == 3) out.type = UniformType::IVec3;
                else if (component_count == 4) out.type = UniformType::IVec4;
            }
            else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL)
            {
                out.type = UniformType::Bool;
            }
            else
            {
                out.type = UniformType::Unknown;
            }
        }
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
        {
            out.type = UniformType::Float;
        }
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_INT)
        {
            if (type_desc->traits.numeric.scalar.signedness)
            {
                out.type = UniformType::Int;
            }
            else
            {
                out.type = UniformType::UInt;
            }
        }
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL)
        {
            out.type = UniformType::Bool;
        }
        else
        {
            out.type = UniformType::Unknown;
        }
    }
    else
    {
        out.type = UniformType::Unknown;
    }

    uint32_t dims_count = 0;
    const uint32_t* dims_ptr = nullptr;

    if (var->array.dims_count > 0)
    {
        dims_count = var->array.dims_count;
        dims_ptr = var->array.dims;
    }
    else if (var->type_description && var->type_description->traits.array.dims_count > 0)
    {
        dims_count = var->type_description->traits.array.dims_count;
        dims_ptr = var->type_description->traits.array.dims;
    }

    if (dims_count > 0 && dims_ptr)
    {
        out.isArray = true;
        out.arrayDims.assign(dims_ptr, dims_ptr + dims_count);
    }
    else
    {
        out.isArray = false;
    }

    if (var->member_count > 0 && var->members)
    {
        out.members.reserve(var->member_count);
        for (uint32_t m = 0; m < var->member_count; ++m)
        {
            UniformMember child;
            ParseBlockVariable(&var->members[m], child);
            out.members.push_back(std::move(child));
        }
    }
}

Uniforms SPV::SpirvReflectUniforms(const std::string& spirv)
{
    size_t spirv_nbytes = spirv.size();
    const void* spirv_code = spirv.data();

    if (spirv_nbytes % sizeof(uint32_t) != 0)
    {
        PrintError("SPIR-V binary is corrupt or truncated");
        return {};
    }

    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirv_nbytes,
                                                           reinterpret_cast<const uint32_t*>(spirv_code),
                                                           &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        PrintError("Failed to create SPIR-V Reflect Shader Module");
        return {};
    }

    uint32_t binding_count = 0;
    result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    SpvReflectDescriptorBinding** bindings =
        static_cast<SpvReflectDescriptorBinding**>(malloc(binding_count * sizeof(SpvReflectDescriptorBinding*)));

    if (bindings == NULL)
    {
        spvReflectDestroyShaderModule(&module);
        PrintError("Failed to allocate memory for SPIR-V Reflect Descriptor Bindings");
        return {};
    }

    result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    Uniforms uniforms;
    uniforms.reserve(binding_count);

    for (uint32_t i = 0; i < binding_count; ++i)
    {
        const SpvReflectDescriptorBinding* binding = bindings[i];
        Uniform u;

        u.name = binding->name ? binding->name : "";
        u.set = binding->set;
        u.binding = binding->binding;

        switch (binding->descriptor_type)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            if (u.name.empty() && binding->type_description)
            {
                u.name = binding->type_description->type_name ? binding->type_description->type_name : u.name;
            }
            u.type = UniformType::NestedStruct;
            u.offset = binding->block.offset;
            u.size = binding->block.size;

            if (binding->block.member_count > 0 && binding->block.members)
            {
                u.members.reserve(binding->block.member_count);
                for (uint32_t m = 0; m < binding->block.member_count; ++m)
                {
                    UniformMember mem;
                    ParseBlockVariable(&binding->block.members[m], mem);
                    u.members.push_back(std::move(mem));
                }
            }
            break;

        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            u.type = UniformType::Sampler2D;
            u.offset = 0;
            u.size = 0; // Samplers don't have a size
            break;

        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            {
                if (u.name.empty() && binding->type_description)
                {
                    u.name = binding->type_description->type_name ? binding->type_description->type_name : u.name;
                }
                u.type = UniformType::StorageBuffer;

                u.offset = binding->block.offset;
                u.size = CalculateStorageBufferSize(binding);

                if (binding->block.member_count > 0 && binding->block.members)
                {
                    u.members.reserve(binding->block.member_count);
                    for (uint32_t m = 0; m < binding->block.member_count; ++m)
                    {
                        UniformMember mem;
                        ParseBlockVariable(&binding->block.members[m], mem);
                        u.members.push_back(std::move(mem));
                    }
                }
                break;
            }

        default:
            u.type = UniformType::Unknown;
            u.size = 0;
            break;
        }

        if (u.type != UniformType::Unknown)
        {
            uniforms[u.name] = std::move(u);
        }
    }

    free(bindings);
    spvReflectDestroyShaderModule(&module);

    return uniforms;
}

std::optional<PushConstant> SPV::SpirvReflectPushConstants(const std::string& spirv)
{
    size_t spirv_nbytes = spirv.size();
    if (spirv_nbytes % sizeof(uint32_t) != 0)
    {
        PrintError("SPIR-V binary is corrupt or truncated");
        return {};
    }

    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(
        spirv_nbytes,
        reinterpret_cast<const uint32_t*>(spirv.data()),
        &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        PrintError("Failed to create SPIR-V Reflect Shader Module");
        return {};
    }

    uint32_t pc_count = 0;
    result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    if (pc_count == 0)
    {
        spvReflectDestroyShaderModule(&module);
        return {};
    }

    SpvReflectBlockVariable** pc_blocks =
        static_cast<SpvReflectBlockVariable**>(
            malloc(pc_count * sizeof(SpvReflectBlockVariable*)));

    if (!pc_blocks)
    {
        PrintError("Allocation failure");
        spvReflectDestroyShaderModule(&module);
        return {};
    }

    result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, pc_blocks);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    assert(pc_count == 1);

    PushConstant pc;
    const SpvReflectBlockVariable* block = pc_blocks[0];

    pc.name = block->name ? block->name : "";
    pc.size = block->size;

    if (block->member_count > 0 && block->members)
    {
        pc.members.reserve(block->member_count);
        for (uint32_t m = 0; m < block->member_count; m++)
        {
            UniformMember mem;
            ParseBlockVariable(&block->members[m], mem);
            pc.members.push_back(std::move(mem));
        }
    }

    free(pc_blocks);
    spvReflectDestroyShaderModule(&module);

    return pc;
}
