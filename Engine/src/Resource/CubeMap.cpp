#include "CubeMap.h"

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
    return true;
}

void CubeMap::Unload()
{
}
