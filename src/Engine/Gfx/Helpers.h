#pragma once

#include <spdlog/spdlog.h>

#include <cstdint>

namespace summer::gfx {

inline bool CheckHRESULT(long hr, const char* message) {
    if (hr < 0) {
        spdlog::error("{} failed (HRESULT=0x{:08X})", message, static_cast<uint32_t>(hr));
        return false;
    }
    return true;
}

} // namespace summer::gfx
