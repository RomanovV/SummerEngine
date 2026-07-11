#pragma once

#include <DirectXMath.h>

namespace summer::gfx {

class Texture2D;

// Minimal surface description for the basic pass. This grows into a full PBR material set
// (metallic/roughness/normal/emissive) in a later phase; for now it carries a base color.
struct Material {
    Texture2D* baseColor = nullptr;
    DirectX::XMFLOAT4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace summer::gfx
