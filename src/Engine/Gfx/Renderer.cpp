#include "Gfx/Renderer.h"
#include "Assets/Image.h"
#include "Assets/ModelLoader.h"
#include "Core/Paths.h"
#include "Gfx/Helpers.h"

#include <DirectXMath.h>
#include <spdlog/spdlog.h>

namespace summer::gfx {

namespace {
// Compiled shaders are staged next to the executable (see scripts/stage_assets.ps1),
// so they resolve identically whether run from the IDE or launched standalone by PIX/RenderDoc.
constexpr const char* kVertexShaderRelative = "shaders/BasicVS.cso";
constexpr const char* kPixelShaderRelative = "shaders/BasicPS.cso";

// Shared CBV/SRV/UAV heap size: ImGui font(s) + engine textures. Grow as content grows.
constexpr uint32_t kSrvHeapCapacity = 1024;
} // namespace

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(Window& window, bool enableDebugLayer) {
    if (!m_device.Initialize(enableDebugLayer)) {
        return false;
    }

    m_viewportWidth = static_cast<uint32_t>(window.GetWidth());
    m_viewportHeight = static_cast<uint32_t>(window.GetHeight());

    if (!m_swapChain.Initialize(
            m_device,
            window.GetNativeHandle(),
            m_viewportWidth,
            m_viewportHeight)) {
        m_device.Shutdown();
        return false;
    }

    if (!m_srvHeap.Initialize(m_device.GetD3D12Device(), kSrvHeapCapacity)) {
        m_swapChain.Shutdown();
        m_device.Shutdown();
        return false;
    }

    // Default base color: a checkerboard so the texturing path is always valid.
    const summer::assets::Image checker = summer::assets::Image::MakeCheckerboard();
    if (!m_defaultTexture.CreateFromImage(m_device, m_srvHeap, checker, /*srgb=*/true)) {
        m_srvHeap.Shutdown();
        m_swapChain.Shutdown();
        m_device.Shutdown();
        return false;
    }

    const std::string vertexShaderPath = summer::Paths::Resolve(kVertexShaderRelative);
    const std::string pixelShaderPath = summer::Paths::Resolve(kPixelShaderRelative);
    if (!m_basicPipeline.Initialize(
            m_device,
            m_swapChain.GetFormat(),
            m_swapChain.GetDepthFormat(),
            vertexShaderPath.c_str(),
            pixelShaderPath.c_str())) {
        m_defaultTexture.Shutdown();
        m_srvHeap.Shutdown();
        m_swapChain.Shutdown();
        m_device.Shutdown();
        return false;
    }

    if (!m_imguiLayer.Initialize(window, m_device, m_swapChain, m_srvHeap)) {
        m_basicPipeline.Shutdown();
        m_defaultTexture.Shutdown();
        m_srvHeap.Shutdown();
        m_swapChain.Shutdown();
        m_device.Shutdown();
        return false;
    }

    m_initialized = true;
    spdlog::info("Renderer initialized");
    return true;
}

Texture2D* Renderer::CreateTexture(const summer::assets::Image& image, bool srgb) {
    if (!m_initialized) {
        return nullptr;
    }
    auto texture = std::make_unique<Texture2D>();
    if (!texture->CreateFromImage(m_device, m_srvHeap, image, srgb)) {
        return nullptr;
    }
    Texture2D* raw = texture.get();
    m_textures.push_back(std::move(texture));
    return raw;
}

Model* Renderer::CreateModel(const summer::assets::MeshData& meshData, Texture2D* fallbackBaseColor) {
    if (!m_initialized) {
        return nullptr;
    }
    auto model = std::make_unique<Model>();
    Texture2D* fallback = fallbackBaseColor ? fallbackBaseColor : &m_defaultTexture;
    if (!model->Load(m_device, m_srvHeap, meshData, fallback)) {
        return nullptr;
    }
    Model* raw = model.get();
    m_models.push_back(std::move(model));
    return raw;
}

Model* Renderer::LoadModel(const std::string& path, Texture2D* fallbackBaseColor) {
    summer::assets::MeshData meshData;
    if (!summer::assets::ModelLoader::LoadFromFile(path, meshData)) {
        return nullptr;
    }
    return CreateModel(meshData, fallbackBaseColor);
}

void Renderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_device.WaitForGpu();

    // Models and textures free their SRV slots, so release them (and ImGui) before the heap.
    m_models.clear();
    m_textures.clear();
    m_defaultTexture.Shutdown();
    m_basicPipeline.Shutdown();

    m_imguiLayer.Shutdown();
    m_srvHeap.Shutdown();
    m_swapChain.Shutdown();
    m_device.Shutdown();
    m_initialized = false;
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
    if (!m_initialized || width == 0 || height == 0) {
        return;
    }

    m_device.WaitForGpu();
    m_swapChain.Resize(m_device, width, height);
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void Renderer::SetClearColor(float r, float g, float b, float a) {
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
}

void* Renderer::GetNativeDevice() const {
    return m_device.GetD3D12Device();
}

void Renderer::BeginImGuiFrame() {
    m_imguiLayer.BeginFrame();
}

float Renderer::Aspect() const {
    return m_viewportHeight > 0
        ? static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight)
        : 1.0f;
}

void Renderer::TransitionResource(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_device.GetCommandList()->ResourceBarrier(1, &barrier);
}

void Renderer::RenderFrame(const summer::scene::Scene& scene) {
    if (!m_initialized) {
        return;
    }

    const uint32_t backBufferIndex = m_swapChain.GetCurrentBackBufferIndex();
    m_device.WaitForBackBuffer(backBufferIndex);

    FrameContext& frame = m_device.GetFrame(backBufferIndex);
    ID3D12Resource* backBuffer = m_swapChain.GetBackBuffer(backBufferIndex);
    if (!backBuffer) {
        return;
    }

    if (!CheckHRESULT(frame.CommandAllocator->Reset(), "Reset command allocator")) {
        return;
    }
    if (!CheckHRESULT(
            m_device.GetCommandList()->Reset(frame.CommandAllocator, nullptr),
            "Reset command list")) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = m_device.GetCommandList();

    TransitionResource(
        backBuffer,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.GetRTV(backBufferIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_swapChain.GetDSV();
    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    const D3D12_VIEWPORT viewport{
        0.0f, 0.0f,
        static_cast<float>(m_viewportWidth),
        static_cast<float>(m_viewportHeight),
        0.0f, 1.0f};
    const D3D12_RECT scissor{
        0, 0, static_cast<LONG>(m_viewportWidth), static_cast<LONG>(m_viewportHeight)};
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    // Bind the shared shader-visible heap so material SRV tables resolve.
    ID3D12DescriptorHeap* heaps[] = {m_srvHeap.Heap()};
    commandList->SetDescriptorHeaps(1, heaps);

    m_basicPipeline.Bind(commandList);

    // view * proj is shared across entities; only the world (and thus MVP) changes per entity.
    const DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(
        scene.GetCamera().GetViewMatrix(),
        scene.GetCamera().GetProjectionMatrix(Aspect()));

    for (const summer::scene::Entity& entity : scene.GetEntities()) {
        if (!entity.model) {
            continue;
        }
        const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&entity.transform);
        const DirectX::XMMATRIX mvp = DirectX::XMMatrixMultiply(world, viewProj);

        DirectX::XMFLOAT4X4 mvpTransposed;
        DirectX::XMStoreFloat4x4(&mvpTransposed, DirectX::XMMatrixTranspose(mvp));

        m_basicPipeline.SetModelViewProj(commandList, mvpTransposed);
        entity.model->Draw(commandList, m_basicPipeline);
    }

    m_imguiLayer.RenderDrawData(commandList);

    TransitionResource(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    if (!CheckHRESULT(commandList->Close(), "Close command list")) {
        return;
    }

    ID3D12CommandList* commandLists[] = {commandList};
    m_device.GetCommandQueue()->ExecuteCommandLists(1, commandLists);

    m_swapChain.Present();
    m_device.SignalBackBuffer(backBufferIndex);
}

} // namespace summer::gfx
