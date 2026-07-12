#include "Scene/Camera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace summer::scene {

XMVECTOR Camera::ComputeEye() const {
    // Spherical to Cartesian offset from the target.
    const float cosPitch = std::cos(m_pitch);
    const XMFLOAT3 offset{
        m_distance * cosPitch * std::sin(m_yaw),
        m_distance * std::sin(m_pitch),
        m_distance * cosPitch * std::cos(m_yaw)};

    const XMVECTOR target = XMLoadFloat3(&m_target);
    return XMVectorAdd(target, XMLoadFloat3(&offset));
}

void Camera::Orbit(float deltaYaw, float deltaPitch) {
    m_yaw += deltaYaw;
    m_pitch = std::clamp(m_pitch + deltaPitch, -m_maxPitch, m_maxPitch);
}

void Camera::Zoom(float delta) {
    // Scale zoom by distance so it feels consistent near and far.
    m_distance = std::clamp(m_distance - delta * m_distance * 0.1f, m_minDistance, m_maxDistance);
}

void Camera::Pan(float deltaRight, float deltaUp) {
    const XMVECTOR eye = ComputeEye();
    const XMVECTOR target = XMLoadFloat3(&m_target);
    const XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, eye));
    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
    const XMVECTOR up = XMVector3Cross(forward, right);

    // Pan speed scales with distance so it tracks the cursor at any zoom.
    const float speed = m_distance * 0.001f;
    XMVECTOR target2 = target;
    target2 = XMVectorAdd(target2, XMVectorScale(right, -deltaRight * speed));
    target2 = XMVectorAdd(target2, XMVectorScale(up, deltaUp * speed));
    XMStoreFloat3(&m_target, target2);
}

XMMATRIX Camera::GetViewMatrix() const {
    const XMVECTOR eye = ComputeEye();
    const XMVECTOR target = XMLoadFloat3(&m_target);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return XMMatrixLookAtLH(eye, target, up);
}

XMMATRIX Camera::GetProjectionMatrix(float aspect) const {
    if (aspect <= 0.0f) {
        aspect = 1.0f;
    }
    return XMMatrixPerspectiveFovLH(m_fovY, aspect, m_nearZ, m_farZ);
}

XMFLOAT3 Camera::GetPosition() const {
    XMFLOAT3 result;
    XMStoreFloat3(&result, ComputeEye());
    return result;
}

} // namespace summer::scene
