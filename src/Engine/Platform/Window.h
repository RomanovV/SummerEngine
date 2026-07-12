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

enum class MouseButton { Left, Right, Middle };

// Subset of keys the engine currently queries (mapped to GLFW in the .cpp).
enum class Key { W, A, S, D, Q, E };

// Thin GLFW wrapper. Creates a Win32 HWND without an OpenGL/Vulkan context
// so D3D12 owns all GPU resource creation via the native handle.
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

    // --- Input queries (polled) ---
    void GetCursorPosition(double& x, double& y) const;
    bool IsMouseButtonDown(MouseButton button) const;
    bool IsKeyDown(Key key) const;
    // Returns accumulated scroll offset since the last call, then resets it.
    double ConsumeScrollDelta();

    // Passed to DXGI CreateSwapChainForHwnd.
    HWND GetNativeHandle() const;
    void* GetGLFWHandle() const { return m_handle; }

private:
    void OnFramebufferResize(int width, int height);

    void* m_handle = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_resized = false;
    double m_scrollAccum = 0.0;
};

} // namespace summer
