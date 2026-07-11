#pragma once

#include "Assets/MeshData.h"
#include "Gfx/BasicPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/GpuMesh.h"
#include "Gfx/ImGuiLayer.h"
#include "Gfx/SwapChain.h"
#include "Platform/Window.h"

#include <cstdint>

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

    void OnResize(uint32_t width, uint32_t height);

    void BeginImGuiFrame();
    void RenderFrame();

    void SetClearColor(float r, float g, float b, float a);
    void SetMeshColor(float r, float g, float b, float a);

private:
    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter);

    DirectX::XMFLOAT4X4 BuildMvpMatrix() const;

    Device m_device;
    SwapChain m_swapChain;
    ImGuiLayer m_imguiLayer;
    BasicPipeline m_basicPipeline;
    GpuMesh m_mesh;

    float m_clearColor[4] = {0.1f, 0.1f, 0.15f, 1.0f};
    float m_meshColor[4] = {0.85f, 0.55f, 0.25f, 1.0f};
    uint32_t m_viewportWidth = 0;
    uint32_t m_viewportHeight = 0;
    bool m_hasMesh = false;
    bool m_initialized = false;
};

} // namespace summer::gfx
