#include "ImageLoader.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <filesystem>
#include <stb/stb_image_write.h>

void ImageLoader::FlipVerticalOnLoad(const bool flagTrueIfShouldFlip)
{
    stbi_set_flip_vertically_on_load(flagTrueIfShouldFlip);
}

bool ImageLoader::Load(char const* filename, Image& image, int req_comp)
{
    int x, y, comp;
    unsigned char* data = stbi_load(filename, &x, &y, &comp, req_comp);

    if (!data)
    {
        image.data = nullptr;
        image.size = Vec2f::Zero();
        image.channels = 0;
        return false;
    }
    
    image.data = data;
    image.size.x = x;
    image.size.y = y;
    image.channels = req_comp;
    return true;
}

bool ImageLoader::Load(const std::filesystem::path& path, Image& image, int req_comp)
{
    return Load(path.generic_string().c_str(), image, req_comp);
}

void ImageLoader::SaveImage(const char* filename, const Image& image)
{
    stbi_write_png(filename, image.size.x, image.size.y, 4, image.data, 4 * image.size.x);
}

bool ImageLoader::LoadHDR(const std::filesystem::path& path, HDRImage& image)
{
    FlipVerticalOnLoad(true);
    int x, y, comp;
    float* data = stbi_loadf(path.generic_string().c_str(), &x, &y, &comp, 0);
    
    if (!data)
    {
        image.data = nullptr;
        image.size = Vec2f::Zero();
        image.channels = 0;
        return false;
    }
    
    image.data = data;
    image.size.x = x;
    image.size.y = y;
    image.channels = comp;
    return true;
}

ImageLoader::Image ImageLoader::LoadFromMemory(unsigned char* data, int len)
{
    Image image;
    int comp;
    image.data = stbi_load_from_memory(data, len, &image.size.x, &image.size.y, &comp, 4);
    image.channels = comp;

    return image;
}


void ImageLoader::ImageFree(void* data)
{
    stbi_image_free(data);
}

void ImageLoader::ImageFree(const HDRImage& image)
{
    stbi_image_free(image.data);
}

void ImageLoader::ImageFree(const Image& image)
{
    stbi_image_free(image.data);
}
