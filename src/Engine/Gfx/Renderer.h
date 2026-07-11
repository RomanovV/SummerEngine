#pragma once

#include "Assets/MeshData.h"
#include "Gfx/BasicPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/GpuDescriptorHeap.h"
#include "Gfx/GpuMesh.h"
#include "Gfx/ImGuiLayer.h"
#include "Gfx/Material.h"
#include "Gfx/SwapChain.h"
#include "Gfx/Texture2D.h"
#include "Platform/Window.h"

#include <cstdint>
#include <string>

namespace summer::gfx {

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Initialize(Window& window, bool enableDebugLayer = true);
    void Shutdown();

    // Uploads CPU mesh data to GPU buffers and initializes the basic draw pipeline.
    bool SetMesh(const summer::assets::MeshData& meshData);

    // Loads an image file as the mesh's base color texture. Falls back to the default
    // checkerboard on failure. Safe to call after Initialize().
    bool LoadBaseColorTexture(const std::string& path, bool srgb = true);

    void OnResize(uint32_t width, uint32_t height);

    void BeginImGuiFrame();
    void RenderFrame();

    void SetClearColor(float r, float g, float b, float a);

    // Native ID3D12Device* for GPU capture tool integration. Null before Initialize().
    void* GetNativeDevice() const;

private:
    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter);

    DirectX::XMFLOAT4X4 BuildMvpMatrix() const;

    Device m_device;
    SwapChain m_swapChain;
    GpuDescriptorHeap m_srvHeap;
    ImGuiLayer m_imguiLayer;
    BasicPipeline m_basicPipeline;
    GpuMesh m_mesh;

    Texture2D m_defaultTexture;
    Texture2D m_baseColorTexture;
    Material m_material;

    float m_clearColor[4] = {0.1f, 0.1f, 0.15f, 1.0f};
    uint32_t m_viewportWidth = 0;
    uint32_t m_viewportHeight = 0;
    bool m_hasMesh = false;
    bool m_initialized = false;
};

} // namespace summer::gfx
