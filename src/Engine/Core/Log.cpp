#include "Core/Log.h"

#include <spdlog/spdlog.h>

namespace summer {

void Log::Init() {
    spdlog::set_pattern("[%T] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::trace);
}

} // namespace summer
