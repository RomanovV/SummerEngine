#pragma once

#include <string>
#include <string_view>

namespace summer {

// Resolves resource paths relative to the running executable rather than the current
// working directory. This lets the app be launched standalone (e.g. by RenderDoc or PIX,
// which set their own working directory) as long as assets/shaders are staged next to the exe.
class Paths {
public:
    // Absolute directory containing the running executable, with a trailing separator.
    static const std::string& ExecutableDir();

    // ExecutableDir() + relative. Forward slashes are accepted and normalized by the OS.
    static std::string Resolve(std::string_view relative);
};

} // namespace summer
