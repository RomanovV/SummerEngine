#pragma once

#include "Assets/MeshData.h"
#include "Gfx/BasicPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/GpuDescriptorHeap.h"
#include "Gfx/ImGuiLayer.h"
#include "Gfx/Model.h"
#include "Gfx/SwapChain.h"
#include "Gfx/Texture2D.h"
#include "Platform/Window.h"
#include "Scene/Scene.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace summer::assets {
struct Image;
}

namespace summer::gfx {

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Initialize(Window& window, bool enableDebugLayer = true);
    void Shutdown();

    // --- Resource creation (owned by the Renderer; returns borrowed pointers) ---
    Texture2D* CreateTexture(const summer::assets::Image& image, bool srgb = true);
    Model* CreateModel(const summer::assets::MeshData& meshData, Texture2D* fallbackBaseColor = nullptr);
    Model* LoadModel(const std::string& path, Texture2D* fallbackBaseColor = nullptr);

    // Default base color used when a material has no texture.
    Texture2D* GetDefaultTexture() { return &m_defaultTexture; }

    void OnResize(uint32_t width, uint32_t height);

    void BeginImGuiFrame();
    // Renders the scene (per entity, per submesh) then the ImGui draw data.
    void RenderFrame(const summer::scene::Scene& scene);

    void SetClearColor(float r, float g, float b, float a);

    // Native ID3D12Device* for GPU capture tool integration. Null before Initialize().
    void* GetNativeDevice() const;

private:
    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter);

    float Aspect() const;

    Device m_device;
    SwapChain m_swapChain;
    GpuDescriptorHeap m_srvHeap;
    ImGuiLayer m_imguiLayer;
    BasicPipeline m_basicPipeline;

    Texture2D m_defaultTexture;
    std::vector<std::unique_ptr<Texture2D>> m_textures;
    std::vector<std::unique_ptr<Model>> m_models;

    float m_clearColor[4] = {0.1f, 0.1f, 0.15f, 1.0f};
    uint32_t m_viewportWidth = 0;
    uint32_t m_viewportHeight = 0;
    bool m_initialized = false;
};

} // namespace summer::gfx
