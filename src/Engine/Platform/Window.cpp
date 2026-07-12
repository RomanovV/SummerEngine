#include "Platform/Window.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <spdlog/spdlog.h>

namespace summer {

namespace {
// GLFW init/terminate must happen exactly once per process.
int s_glfwRefCount = 0;
} // namespace

Window::Window(const WindowConfig& config) {
    if (s_glfwRefCount++ == 0) {
        if (!glfwInit()) {
            spdlog::critical("Failed to initialize GLFW");
            return;
        }
    }

    // No graphics context -- we only need the HWND for DXGI.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    auto* window = glfwCreateWindow(
        config.width,
        config.height,
        config.title.c_str(),
        nullptr,
        nullptr);

    if (!window) {
        spdlog::critical("Failed to create GLFW window");
        if (--s_glfwRefCount == 0) {
            glfwTerminate();
        }
        return;
    }

    m_handle = window;
    m_width = config.width;
    m_height = config.height;

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* glfwWindow, int width, int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
        if (self) {
            self->OnFramebufferResize(width, height);
        }
    });
    glfwSetScrollCallback(window, [](GLFWwindow* glfwWindow, double /*xoffset*/, double yoffset) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
        if (self) {
            self->m_scrollAccum += yoffset;
        }
    });

    spdlog::info("Window created ({}x{})", m_width, m_height);
}

Window::~Window() {
    if (m_handle) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(m_handle));
        m_handle = nullptr;
    }

    if (s_glfwRefCount > 0 && --s_glfwRefCount == 0) {
        glfwTerminate();
    }
}

bool Window::ShouldClose() const {
    if (!m_handle) {
        return true;
    }
    return glfwWindowShouldClose(static_cast<GLFWwindow*>(m_handle)) != 0;
}

void Window::PollEvents() {
    glfwPollEvents();
}

HWND Window::GetNativeHandle() const {
    if (!m_handle) {
        return nullptr;
    }
    return glfwGetWin32Window(static_cast<GLFWwindow*>(m_handle));
}

void Window::GetCursorPosition(double& x, double& y) const {
    x = 0.0;
    y = 0.0;
    if (m_handle) {
        glfwGetCursorPos(static_cast<GLFWwindow*>(m_handle), &x, &y);
    }
}

bool Window::IsMouseButtonDown(MouseButton button) const {
    if (!m_handle) {
        return false;
    }
    int glfwButton = GLFW_MOUSE_BUTTON_LEFT;
    switch (button) {
        case MouseButton::Left: glfwButton = GLFW_MOUSE_BUTTON_LEFT; break;
        case MouseButton::Right: glfwButton = GLFW_MOUSE_BUTTON_RIGHT; break;
        case MouseButton::Middle: glfwButton = GLFW_MOUSE_BUTTON_MIDDLE; break;
    }
    return glfwGetMouseButton(static_cast<GLFWwindow*>(m_handle), glfwButton) == GLFW_PRESS;
}

bool Window::IsKeyDown(Key key) const {
    if (!m_handle) {
        return false;
    }
    int glfwKey = GLFW_KEY_W;
    switch (key) {
        case Key::W: glfwKey = GLFW_KEY_W; break;
        case Key::A: glfwKey = GLFW_KEY_A; break;
        case Key::S: glfwKey = GLFW_KEY_S; break;
        case Key::D: glfwKey = GLFW_KEY_D; break;
        case Key::Q: glfwKey = GLFW_KEY_Q; break;
        case Key::E: glfwKey = GLFW_KEY_E; break;
    }
    return glfwGetKey(static_cast<GLFWwindow*>(m_handle), glfwKey) == GLFW_PRESS;
}

double Window::ConsumeScrollDelta() {
    const double delta = m_scrollAccum;
    m_scrollAccum = 0.0;
    return delta;
}

void Window::OnFramebufferResize(int width, int height) {
    m_width = width;
    m_height = height;
    m_resized = true;
}

} // namespace summer
