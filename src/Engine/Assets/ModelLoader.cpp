#include "Assets/ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>

namespace summer::assets {

namespace {

struct AccumulatedMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> subMeshes;
};

void ApplyMatrixToVertex(Vertex& vertex, const aiMatrix4x4& matrix) {
    aiVector3D position(vertex.position[0], vertex.position[1], vertex.position[2]);
    aiVector3D normal(vertex.normal[0], vertex.normal[1], vertex.normal[2]);

    position = matrix * position;
    normal = matrix * normal;
    if (normal.SquareLength() > 0.0f) {
        normal.Normalize();
    }

    vertex.position[0] = position.x;
    vertex.position[1] = position.y;
    vertex.position[2] = position.z;
    vertex.normal[0] = normal.x;
    vertex.normal[1] = normal.y;
    vertex.normal[2] = normal.z;
}

void ProcessMesh(
    const aiMesh* mesh,
    const aiScene* scene,
    const aiMatrix4x4& transform,
    AccumulatedMesh& accumulated) {
    if (!mesh || !mesh->HasPositions()) {
        return;
    }

    SubMesh subMesh;
    subMesh.name = mesh->mName.C_Str();
    subMesh.materialIndex = mesh->mMaterialIndex;
    subMesh.indexOffset = static_cast<uint32_t>(accumulated.indices.size());

    const uint32_t baseVertex = static_cast<uint32_t>(accumulated.vertices.size());

    accumulated.vertices.resize(baseVertex + mesh->mNumVertices);
    for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
        Vertex& vertex = accumulated.vertices[baseVertex + vertexIndex];

        const aiVector3D& position = mesh->mVertices[vertexIndex];
        vertex.position[0] = position.x;
        vertex.position[1] = position.y;
        vertex.position[2] = position.z;

        if (mesh->HasNormals()) {
            const aiVector3D& normal = mesh->mNormals[vertexIndex];
            vertex.normal[0] = normal.x;
            vertex.normal[1] = normal.y;
            vertex.normal[2] = normal.z;
        }

        if (mesh->HasTextureCoords(0)) {
            const aiVector3D& uv = mesh->mTextureCoords[0][vertexIndex];
            vertex.uv[0] = uv.x;
            vertex.uv[1] = uv.y;
        }

        ApplyMatrixToVertex(vertex, transform);
    }

    for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
        const aiFace& face = mesh->mFaces[faceIndex];
        if (face.mNumIndices != 3) {
            // Triangulate flag should guarantee triangles, but guard anyway.
            continue;
        }

        for (uint32_t index = 0; index < face.mNumIndices; ++index) {
            accumulated.indices.push_back(baseVertex + face.mIndices[index]);
        }
    }

    subMesh.indexCount = static_cast<uint32_t>(accumulated.indices.size()) - subMesh.indexOffset;
    if (subMesh.indexCount > 0) {
        accumulated.subMeshes.push_back(std::move(subMesh));
    }
}

void ProcessNode(
    const aiNode* node,
    const aiScene* scene,
    const aiMatrix4x4& parentTransform,
    AccumulatedMesh& accumulated) {
    const aiMatrix4x4 worldTransform = parentTransform * node->mTransformation;

    for (uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
        ProcessMesh(mesh, scene, worldTransform, accumulated);
    }

    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        ProcessNode(node->mChildren[childIndex], scene, worldTransform, accumulated);
    }
}

uint32_t BuildPostProcessFlags(uint32_t extraFlags) {
    // Baseline cleanup suitable for most sandbox experiments.
    uint32_t flags = aiProcess_Triangulate |
                     aiProcess_GenNormals |
                     aiProcess_JoinIdenticalVertices |
                     aiProcess_ImproveCacheLocality |
                     aiProcess_GenUVCoords;
    return flags | extraFlags;
}

} // namespace

bool ModelLoader::LoadFromFile(
    const std::string& path,
    MeshData& outMesh,
    uint32_t extraPostProcessFlags) {
    outMesh = {};

    Assimp::Importer importer;
    const uint32_t postProcessFlags = BuildPostProcessFlags(extraPostProcessFlags);

    const aiScene* scene = importer.ReadFile(path.c_str(), postProcessFlags);
    if (!scene || !scene->mRootNode) {
        spdlog::error("Assimp failed to load '{}': {}", path, importer.GetErrorString());
        return false;
    }

    AccumulatedMesh accumulated;
    ProcessNode(scene->mRootNode, scene, aiMatrix4x4(), accumulated);

    if (accumulated.vertices.empty() || accumulated.indices.empty()) {
        spdlog::error("Assimp loaded '{}' but produced no geometry", path);
        return false;
    }

    outMesh.vertices = std::move(accumulated.vertices);
    outMesh.indices = std::move(accumulated.indices);
    outMesh.subMeshes = std::move(accumulated.subMeshes);
    outMesh.sourcePath = path;

    spdlog::info(
        "Loaded mesh '{}' — {} vertices, {} indices, {} submesh(es)",
        path,
        outMesh.vertexCount(),
        outMesh.indexCount(),
        outMesh.subMeshes.size());

    for (const SubMesh& subMesh : outMesh.subMeshes) {
        spdlog::info(
            "  SubMesh '{}' — {} indices, material {}",
            subMesh.name,
            subMesh.indexCount,
            subMesh.materialIndex);
    }

    return true;
}

} // namespace summer::assets
