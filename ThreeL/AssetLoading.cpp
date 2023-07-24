#include "pch.h"
#include "AssetLoading.h"

#include <stb_image.h>

Scene LoadGltfScene(ResourceManager& resources, const std::string& filePath, const float4x4& transform)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string gltfError;
    std::string gltfWarning;
    printf("Parsing glTF file...\n");
    bool success = loader.LoadASCIIFromFile(&model, &gltfError, &gltfWarning, filePath);

    if (!gltfError.empty())
        printf("glTF parsing error: %s\n", gltfError.c_str());

    if (!gltfWarning.empty())
        printf("glTF parsing warning: %s\n", gltfWarning.c_str());

    if (!success)
    {
        printf("glTF parsing failed.\n");
        exit(1);
    }

    printf("Loading glTF scene...\n");
    return Scene(resources, model, transform);
}

Texture LoadHdr(ResourceManager& resources, std::string filePath)
{
    int width;
    int height;
    int channels;
    // Forcing 4 channels because R32G32B32 cannot be accessed via UAV which is required for mipmap chain generation.
    float* data = stbi_loadf(filePath.c_str(), &width, &height, &channels, 4);
    Assert(data != nullptr);
    channels = 4;
    Texture texture(resources, std::format(L"{}", filePath), std::span(data, width * height * channels), uint2((uint32_t)width, (uint32_t)height), channels);
    stbi_image_free(data);
    return texture;
}

Texture LoadTexture(ResourceManager& resources, std::string filePath)
{
    int width;
    int height;
    int channels;
    uint8_t* data = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
    Assert(data != nullptr);
    channels = 4;
    Texture texture(resources, std::format(L"{}", filePath), std::span(data, width * height * channels), uint2((uint32_t)width, (uint32_t)height), channels);
    stbi_image_free(data);
    return texture;
}
