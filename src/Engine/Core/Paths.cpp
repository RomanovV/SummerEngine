#include "Core/Paths.h"

#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace summer {

const std::string& Paths::ExecutableDir() {
    static const std::string dir = [] {
        std::string result;
#if defined(_WIN32)
        wchar_t buffer[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::wstring widePath(buffer, length);

        const size_t lastSlash = widePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            widePath.resize(lastSlash + 1);
        }

        const int bytes = WideCharToMultiByte(
            CP_UTF8, 0, widePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (bytes > 0) {
            result.resize(static_cast<size_t>(bytes - 1));
            WideCharToMultiByte(
                CP_UTF8, 0, widePath.c_str(), -1, result.data(), bytes, nullptr, nullptr);
        }
#endif
        if (result.empty()) {
            result = "./";
        }
        return result;
    }();
    return dir;
}

std::string Paths::Resolve(std::string_view relative) {
    return ExecutableDir() + std::string(relative);
}

} // namespace summer
