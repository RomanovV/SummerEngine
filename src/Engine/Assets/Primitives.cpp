#include "Assets/Primitives.h"

namespace summer::assets {

MeshData Primitives::MakePlane(float halfSize, float uvTiling) {
    MeshData mesh;
    mesh.sourcePath = "<primitive:plane>";

    const float h = halfSize;
    const float t = uvTiling;

    // Wound counter-clockwise viewed from above (+Y), consistent with the engine's cube.
    // Tangent = +X (u direction); w = -1 so bitangent = cross(N,T)*w points to +Z (v direction).
    const float tan[4] = {1.0f, 0.0f, 0.0f, -1.0f};
    mesh.vertices = {
        {{-h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {tan[0], tan[1], tan[2], tan[3]}, {0.0f, 0.0f}},
        {{-h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, {tan[0], tan[1], tan[2], tan[3]}, {0.0f, t}},
        {{h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, {tan[0], tan[1], tan[2], tan[3]}, {t, t}},
        {{h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {tan[0], tan[1], tan[2], tan[3]}, {t, 0.0f}},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};

    SubMesh subMesh;
    subMesh.name = "Plane";
    subMesh.indexOffset = 0;
    subMesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
    subMesh.materialIndex = 0;
    mesh.subMeshes.push_back(subMesh);

    return mesh;
}

} // namespace summer::assets
