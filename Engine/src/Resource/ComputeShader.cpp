#include "ComputeShader.h"

ComputeDispatch::~ComputeDispatch()
{
    if (m_buffer)
    {
        m_buffer->Cleanup();
        m_buffer.reset();
    }
}

bool ComputeShader::SendToGPU(VulkanRenderer* renderer)
{
    if (!BaseShader::SendToGPU(renderer))
        return false;
    
    return true;
}
