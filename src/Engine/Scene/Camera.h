#pragma once

#include <DirectXMath.h>

namespace summer::scene {

// Orbit camera: looks at a target point from a spherical position (yaw/pitch/distance).
// Left-handed to match the D3D projection used across the engine.
class Camera {
public:
    void Orbit(float deltaYaw, float deltaPitch);
    void Zoom(float delta);                       // positive = closer
    void Pan(float deltaRight, float deltaUp);    // moves the target in the view plane

    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix(float aspect) const;
    DirectX::XMFLOAT3 GetPosition() const;

    void SetTarget(const DirectX::XMFLOAT3& target) { m_target = target; }
    void SetDistance(float distance) { m_distance = distance; }

    float GetFovY() const { return m_fovY; }
    void SetFovY(float fovY) { m_fovY = fovY; }

private:
    DirectX::XMVECTOR ComputeEye() const;

    DirectX::XMFLOAT3 m_target{0.0f, 0.0f, 0.0f};
    float m_distance = 8.0f;
    float m_yaw = -0.6f;    // radians, around +Y
    float m_pitch = 0.5f;   // radians, above the horizon
    float m_fovY = DirectX::XM_PIDIV4;
    float m_nearZ = 0.1f;
    float m_farZ = 500.0f;

    float m_minDistance = 1.5f;
    float m_maxDistance = 200.0f;
    float m_maxPitch = DirectX::XM_PIDIV2 - 0.05f;
};

} // namespace summer::scene
