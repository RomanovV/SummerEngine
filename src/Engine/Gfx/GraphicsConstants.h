#pragma once

#include <cstdint>

namespace summer::gfx {

// Keep in sync with kMaxCascades in shaders/DeferredLightingPS.hlsl.
inline constexpr uint32_t kMaxCascades = 8;

} // namespace summer::gfx
