#include "Gfx/Renderer.h"
#include "Core/Paths.h"
#include "Gfx/Helpers.h"

#include <DirectXMath.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>

namespace summer::gfx {

namespace {
// Compiled shaders are staged next to the executable (see scripts/stage_assets.ps1),
// so they resolve identically whether run from the IDE or launched standalone by PIX/RenderDoc.
constexpr const char* kVertexShaderRelative = "shaders/BasicVS.cso";
constexpr const char* kPixelShaderRelative = "shaders/BasicPS.cso";
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

    if (!m_imguiLayer.Initialize(window, m_device, m_swapChain)) {
        m_swapChain.Shutdown();
        m_device.Shutdown();
        return false;
    }

    m_initialized = true;
    spdlog::info("Renderer initialized");
    return true;
}

bool Renderer::SetMesh(const summer::assets::MeshData& meshData) {
    if (!m_initialized) {
        return false;
    }

    if (!m_mesh.Initialize(m_device, meshData)) {
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
        m_mesh.Shutdown();
        return false;
    }

    m_hasMesh = true;
    spdlog::info("Renderer mesh pipeline ready");
    return true;
}

void Renderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_device.WaitForGpu();

    m_hasMesh = false;
    m_mesh.Shutdown();
    m_basicPipeline.Shutdown();
    m_imguiLayer.Shutdown();
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

void Renderer::SetMeshColor(float r, float g, float b, float a) {
    m_meshColor[0] = r;
    m_meshColor[1] = g;
    m_meshColor[2] = b;
    m_meshColor[3] = a;
}

void Renderer::BeginImGuiFrame() {
    m_imguiLayer.BeginFrame();
}

DirectX::XMFLOAT4X4 Renderer::BuildMvpMatrix() const {
    using namespace DirectX;

    static auto startTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const float elapsedSeconds =
        std::chrono::duration<float>(now - startTime).count();

    const float aspect = m_viewportHeight > 0
        ? static_cast<float>(m_viewportWidth) / static_cast<float>(m_viewportHeight)
        : 1.0f;

    const XMMATRIX model = XMMatrixRotationY(elapsedSeconds * 0.8f);
    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(3.0f, 2.5f, -4.0f, 0.0f),
        XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);
    const XMMATRIX mvp = XMMatrixMultiply(XMMatrixMultiply(model, view), projection);

    XMFLOAT4X4 result;
    XMStoreFloat4x4(&result, XMMatrixTranspose(mvp));
    return result;
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

void Renderer::RenderFrame() {
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

    TransitionResource(
        backBuffer,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.GetRTV(backBufferIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_swapChain.GetDSV();
    m_device.GetCommandList()->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    m_device.GetCommandList()->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);
    m_device.GetCommandList()->ClearDepthStencilView(
        dsv,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);

    if (m_hasMesh) {
        const D3D12_VIEWPORT viewport{
            0.0f,
            0.0f,
            static_cast<float>(m_viewportWidth),
            static_cast<float>(m_viewportHeight),
            0.0f,
            1.0f};
        const D3D12_RECT scissor{0, 0, static_cast<LONG>(m_viewportWidth), static_cast<LONG>(m_viewportHeight)};

        m_device.GetCommandList()->RSSetViewports(1, &viewport);
        m_device.GetCommandList()->RSSetScissorRects(1, &scissor);

        m_basicPipeline.Bind(m_device.GetCommandList());

        const DirectX::XMFLOAT4X4 mvp = BuildMvpMatrix();
        const DirectX::XMFLOAT4 meshColor(m_meshColor[0], m_meshColor[1], m_meshColor[2], m_meshColor[3]);
        m_basicPipeline.UpdateConstants(m_device.GetCommandList(), mvp, meshColor);

        m_mesh.Draw(m_device.GetCommandList());
    }

    m_imguiLayer.RenderDrawData(m_device.GetCommandList());

    TransitionResource(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    if (!CheckHRESULT(m_device.GetCommandList()->Close(), "Close command list")) {
        return;
    }

    ID3D12CommandList* commandLists[] = {m_device.GetCommandList()};
    m_device.GetCommandQueue()->ExecuteCommandLists(1, commandLists);

    m_swapChain.Present();
    m_device.SignalBackBuffer(backBufferIndex);
}

} // namespace summer::gfx
