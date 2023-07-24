#pragma once
#include "Matrix4.h"
#include "Scene.h"
#include "Texture.h"

struct ResourceManager;

Scene LoadGltfScene(ResourceManager& resources, const std::string& filePath, const float4x4& transform = float4x4::Identity);
Texture LoadHdr(ResourceManager& resources, std::string filePath);
Texture LoadTexture(ResourceManager& resources, std::string filePath);
