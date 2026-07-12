#pragma once

#include <string>

namespace summer::assets {

// CPU-side material description imported from a model file. Grows toward full PBR
// (metallic/roughness/normal/emissive) in a later phase; for now it carries base color.
struct MaterialData {
    std::string name;
    std::string baseColorTexturePath; // absolute/resolved path, empty if none
    float baseColorFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    bool HasBaseColorTexture() const { return !baseColorTexturePath.empty(); }
};

} // namespace summer::assets
