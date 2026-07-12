#pragma once

#include "Scene/Camera.h"

#include <DirectXMath.h>
#include <vector>

namespace summer::gfx {
class Model;
}

namespace summer::scene {

// A single placed instance of a model in the world.
struct Entity {
    summer::gfx::Model* model = nullptr; // borrowed; owned by the Renderer
    DirectX::XMFLOAT4X4 transform;       // row-major world matrix

    Entity() { DirectX::XMStoreFloat4x4(&transform, DirectX::XMMatrixIdentity()); }
    Entity(summer::gfx::Model* m, const DirectX::XMMATRIX& worldMatrix) : model(m) {
        DirectX::XMStoreFloat4x4(&transform, worldMatrix);
    }
};

// Minimal flat scene: a camera plus a list of entities. No hierarchy/serialization yet.
class Scene {
public:
    Camera& GetCamera() { return m_camera; }
    const Camera& GetCamera() const { return m_camera; }

    void AddEntity(summer::gfx::Model* model, const DirectX::XMMATRIX& worldMatrix) {
        m_entities.emplace_back(model, worldMatrix);
    }
    void Clear() { m_entities.clear(); }

    const std::vector<Entity>& GetEntities() const { return m_entities; }

private:
    Camera m_camera;
    std::vector<Entity> m_entities;
};

} // namespace summer::scene
