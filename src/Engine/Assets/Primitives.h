#pragma once

#include "Assets/MeshData.h"

namespace summer::assets {

// Procedural mesh generators for quick scene setup / testing.
class Primitives {
public:
    // A flat quad on the XZ plane (normal +Y) centered at the origin, spanning [-halfSize, halfSize].
    // uvTiling repeats the texture coordinates across the surface.
    static MeshData MakePlane(float halfSize = 10.0f, float uvTiling = 8.0f);
};

} // namespace summer::assets
