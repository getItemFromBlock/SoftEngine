#include "FragmentShader.h"

bool FragmentShader::Load(ResourceManager* resourceManager)
{
    return BaseShader::Load(resourceManager);
}

bool FragmentShader::SendToGPU(VulkanRenderer* renderer)
{
    return BaseShader::SendToGPU(renderer);
}

void FragmentShader::Unload()
{
}
