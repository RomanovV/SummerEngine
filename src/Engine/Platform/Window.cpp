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

void Window::OnFramebufferResize(int width, int height) {
    m_width = width;
    m_height = height;
    m_resized = true;
}

} // namespace summer
