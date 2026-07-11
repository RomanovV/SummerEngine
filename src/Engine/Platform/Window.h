#pragma once

#include <cstdint>
#include <string>

struct HWND__;
using HWND = HWND__*;

namespace summer {

struct WindowConfig {
    std::string title = "Summer Sandbox";
    int width = 1280;
    int height = 720;
    bool resizable = true;
};

class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const;
    void PollEvents();

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool WasResized() const { return m_resized; }
    void ClearResizedFlag() { m_resized = false; }

    HWND GetNativeHandle() const;
    void* GetGLFWHandle() const { return m_handle; }

private:
    void OnFramebufferResize(int width, int height);

    void* m_handle = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_resized = false;
};

} // namespace summer
