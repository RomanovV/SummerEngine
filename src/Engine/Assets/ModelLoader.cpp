#include "Assets/ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <initializer_list>

namespace summer::assets {

namespace {

// Returns the directory portion of a path (with trailing separator), or empty.
std::string DirectoryOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return {};
    }
    return path.substr(0, slash + 1);
}

std::string ResolveTexturePath(const std::string& baseDir, const aiString& texPath) {
    std::string relative = texPath.C_Str();
    if (relative.empty()) {
        return {};
    }
    // Embedded textures are referenced as "*0"; we don't support those here yet.
    if (relative[0] == '*') {
        return {};
    }
    std::replace(relative.begin(), relative.end(), '\\', '/');
    // Absolute paths (drive letter or leading slash) are used as-is.
    const bool isAbsolute =
        (relative.size() > 1 && relative[1] == ':') || relative[0] == '/';
    return isAbsolute ? relative : baseDir + relative;
}

// Resolves the first available texture among the given types, or empty.
std::string FirstTexture(
    const aiMaterial* material,
    const std::string& baseDir,
    std::initializer_list<aiTextureType> types) {
    aiString texPath;
    for (aiTextureType type : types) {
        if (material->GetTexture(type, 0, &texPath) == AI_SUCCESS) {
            std::string resolved = ResolveTexturePath(baseDir, texPath);
            if (!resolved.empty()) {
                return resolved;
            }
        }
    }
    return {};
}

void ImportMaterials(const aiScene* scene, const std::string& baseDir, MeshData& outMesh) {
    outMesh.materials.reserve(scene->mNumMaterials);
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        const aiMaterial* material = scene->mMaterials[materialIndex];
        MaterialData data;

        aiString name;
        if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            data.name = name.C_Str();
        }

        aiColor4D baseColor;
        if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS ||
            material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS) {
            data.baseColorFactor[0] = baseColor.r;
            data.baseColorFactor[1] = baseColor.g;
            data.baseColorFactor[2] = baseColor.b;
            data.baseColorFactor[3] = baseColor.a;
        }

        aiColor3D emissive;
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
            data.emissiveFactor[0] = emissive.r;
            data.emissiveFactor[1] = emissive.g;
            data.emissiveFactor[2] = emissive.b;
        }

        float metallic = 1.0f;
        if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
            data.metallicFactor = metallic;
        }
        float roughness = 1.0f;
        if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
            data.roughnessFactor = roughness;
        }

        // glTF base color -> BASE_COLOR (legacy DIFFUSE fallback).
        data.baseColorTexturePath =
            FirstTexture(material, baseDir, {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE});
        // Tangent-space normal map.
        data.normalTexturePath =
            FirstTexture(material, baseDir, {aiTextureType_NORMALS, aiTextureType_HEIGHT});
        // Packed metallic-roughness (Assimp exposes glTF's as METALNESS / DIFFUSE_ROUGHNESS / UNKNOWN).
        data.metalRoughTexturePath = FirstTexture(
            material,
            baseDir,
            {aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_UNKNOWN});
        // Occlusion (frequently the same ORM file as metalRough).
        data.occlusionTexturePath = FirstTexture(
            material, baseDir, {aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP});
        data.emissiveTexturePath = FirstTexture(material, baseDir, {aiTextureType_EMISSIVE});

        outMesh.materials.push_back(std::move(data));
    }
}

struct AccumulatedMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> subMeshes;
};

void ApplyMatrixToVertex(Vertex& vertex, const aiMatrix4x4& matrix) {
    aiVector3D position(vertex.position[0], vertex.position[1], vertex.position[2]);
    aiVector3D normal(vertex.normal[0], vertex.normal[1], vertex.normal[2]);

    // Normals/tangents transform by the upper 3x3 (rotation/scale), not the translation.
    aiMatrix3x3 linear(matrix);
    aiVector3D tangent(vertex.tangent[0], vertex.tangent[1], vertex.tangent[2]);

    position = matrix * position;
    normal = linear * normal;
    tangent = linear * tangent;
    if (normal.SquareLength() > 0.0f) {
        normal.Normalize();
    }
    if (tangent.SquareLength() > 0.0f) {
        tangent.Normalize();
    }

    vertex.position[0] = position.x;
    vertex.position[1] = position.y;
    vertex.position[2] = position.z;
    vertex.normal[0] = normal.x;
    vertex.normal[1] = normal.y;
    vertex.normal[2] = normal.z;
    vertex.tangent[0] = tangent.x;
    vertex.tangent[1] = tangent.y;
    vertex.tangent[2] = tangent.z;
    // tangent[3] (handedness) is preserved.
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

        if (mesh->HasTangentsAndBitangents()) {
            const aiVector3D& tangent = mesh->mTangents[vertexIndex];
            const aiVector3D& bitangent = mesh->mBitangents[vertexIndex];
            const aiVector3D& normal = mesh->mNormals[vertexIndex];
            vertex.tangent[0] = tangent.x;
            vertex.tangent[1] = tangent.y;
            vertex.tangent[2] = tangent.z;
            // Handedness: sign of the frame so the shader can reconstruct the bitangent.
            const aiVector3D computedBitangent = normal ^ tangent; // cross(N, T)
            vertex.tangent[3] = (computedBitangent * bitangent) < 0.0f ? -1.0f : 1.0f;
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
                     aiProcess_GenSmoothNormals |
                     aiProcess_CalcTangentSpace |
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

    ImportMaterials(scene, DirectoryOf(path), outMesh);

    spdlog::info(
        "Loaded mesh '{}' - {} vertices, {} indices, {} submesh(es), {} material(s)",
        path,
        outMesh.vertexCount(),
        outMesh.indexCount(),
        outMesh.subMeshes.size(),
        outMesh.materials.size());

    for (const SubMesh& subMesh : outMesh.subMeshes) {
        spdlog::info(
            "  SubMesh '{}' - {} indices, material {}",
            subMesh.name,
            subMesh.indexCount,
            subMesh.materialIndex);
    }

    return true;
}

} // namespace summer::assets
