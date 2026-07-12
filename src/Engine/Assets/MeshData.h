#pragma once

#include "Assets/MaterialData.h"

#include <cstdint>
#include <string>
#include <vector>

namespace summer::assets {

// CPU-side vertex layout. This is the contract between asset import and GPU upload (Step 7).
// When you add tangents, bone weights, etc., extend this struct and update ModelLoader.
struct Vertex {
    float position[3] = {};
    float normal[3] = {};
    float uv[2] = {};
};

// Range into the shared vertex/index arrays for one aiMesh / draw group.
struct SubMesh {
    std::string name;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

// All geometry from a loaded file, flattened into single CPU buffers. SubMeshes keep their
// material index so each can be drawn with its own material.
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> subMeshes;
    std::vector<MaterialData> materials;

    std::string sourcePath;
    uint32_t vertexCount() const { return static_cast<uint32_t>(vertices.size()); }
    uint32_t indexCount() const { return static_cast<uint32_t>(indices.size()); }
    bool IsValid() const { return !vertices.empty() && !indices.empty(); }
};

} // namespace summer::assets
