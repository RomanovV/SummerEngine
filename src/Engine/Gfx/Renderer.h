#pragma once

#include "Assets/MeshData.h"
#include "Gfx/ConstantBuffer.h"
#include "Gfx/CpuDescriptorHeap.h"
#include "Gfx/DepthStencil.h"
#include "Gfx/DeferredLightingPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/GBuffer.h"
#include "Gfx/GeometryPipeline.h"
#include "Gfx/GpuDescriptorHeap.h"
#include "Gfx/ImGuiLayer.h"
#include "Gfx/Lighting.h"
#include "Gfx/Model.h"
#include "Gfx/PbrPipeline.h"
#include "Gfx/RenderTarget.h"
#include "Gfx/SwapChain.h"
#include "Gfx/Texture2D.h"
#include "Gfx/TonemapPipeline.h"
#include "Platform/Window.h"
#include "Scene/Scene.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace summer::assets {
struct Image;
}

namespace summer::gfx {

// Editable scene lighting state (driven by the sandbox UI, consumed each frame).
struct SceneLighting {
    DirectionalLightGpu directional;
    PointLightGpu points[kMaxPointLights];
    int pointCount = 0;
    DirectX::XMFLOAT3 ambient{0.03f, 0.03f, 0.04f};
};

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
    Model* CreateModel(const summer::assets::MeshData& meshData);
    Model* LoadModel(const std::string& path);

    void OnResize(uint32_t width, uint32_t height);

    void BeginImGuiFrame();
    // Geometry into the G-buffer, deferred lighting into HDR, then tonemap + ImGui.
    void RenderFrame(const summer::scene::Scene& scene);

    // --- Lighting / tonemap controls (for the UI) ---
    SceneLighting& Lighting() { return m_lighting; }
    float& Exposure() { return m_exposure; }
    TonemapPipeline::Operator& TonemapOperator() { return m_tonemapOp; }

    // Native ID3D12Device* for GPU capture tool integration. Null before Initialize().
    void* GetNativeDevice() const;

private:
    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter);

    bool CreateDefaultTextures();
    float Aspect() const;

    Device m_device;
    SwapChain m_swapChain;
    GpuDescriptorHeap m_srvHeap;  // shader-visible CBV/SRV/UAV (global)
    CpuDescriptorHeap m_rtvHeap;  // CPU-only RTV pool for offscreen targets (global)
    CpuDescriptorHeap m_dsvHeap;  // CPU-only DSV pool (global)
    ImGuiLayer m_imguiLayer;

    PbrPipeline m_pbrPipeline;
    GeometryPipeline m_geometryPipeline;
    DeferredLightingPipeline m_deferredLighting;
    TonemapPipeline m_tonemapPipeline;
    RenderTarget m_hdrTarget;
    DepthStencil m_depth; // forward pass; the G-buffer owns the sampleable depth used later
    GBuffer m_gbuffer;
    ConstantBuffer m_frameCb;

    // Procedural fallback textures (one per PBR slot) + the bundle handed to models.
    Texture2D m_defaultBaseColor;
    Texture2D m_defaultNormal;
    Texture2D m_defaultMetalRough;
    Texture2D m_defaultOcclusion;
    Texture2D m_defaultEmissive;
    MaterialDefaults m_materialDefaults;

    std::vector<std::unique_ptr<Texture2D>> m_textures;
    std::vector<std::unique_ptr<Model>> m_models;

    SceneLighting m_lighting;
    float m_exposure = 1.0f;
    TonemapPipeline::Operator m_tonemapOp = TonemapPipeline::Operator::ACES;

    float m_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    uint32_t m_viewportWidth = 0;
    uint32_t m_viewportHeight = 0;
    bool m_initialized = false;
};

} // namespace summer::gfx
