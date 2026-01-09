#include "VertexShader.h"

#include "Debug/Log.h"

bool VertexShader::Load(ResourceManager* resourceManager)
{
    return BaseShader::Load(resourceManager);
}

bool VertexShader::SendToGPU(VulkanRenderer* renderer)
{
    return BaseShader::SendToGPU(renderer);
}

void VertexShader::Unload()
{
    BaseShader::Unload();
}
