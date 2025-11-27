#include "Shader.h"

bool BaseShader::Load(ResourceManager* resourceManager)
{
    return true;
}

bool BaseShader::SendToGPU(RHIRenderer* renderer)
{
    return true;
}

bool Shader::Load(ResourceManager* resourceManager)
{
    return true;
}

bool Shader::SendToGPU(RHIRenderer* renderer)
{
    return true;
}

void Shader::Unload()
{
}
