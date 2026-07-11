#pragma once

#include "Assets/MeshData.h"

#include <cstdint>
#include <string>

namespace summer::assets {

class ModelLoader {
public:
    // Default post-process flags applied on import. Override for experiments (e.g. skip GenNormals).
    static constexpr uint32_t DefaultPostProcessFlags = 0;

    // Loads all meshes in the file, applies node transforms, and merges into one MeshData.
    static bool LoadFromFile(
        const std::string& path,
        MeshData& outMesh,
        uint32_t extraPostProcessFlags = DefaultPostProcessFlags);
};

} // namespace summer::assets
