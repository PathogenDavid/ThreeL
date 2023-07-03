#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#pragma warning (push)
#pragma warning (disable : 4018) // signed/unsigned mismatch
#include <tiny_gltf.h>
#pragma warning (pop)
