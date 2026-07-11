#include "Gfx/SwapChain.h"
#include "Gfx/Helpers.h"

#include <spdlog/spdlog.h>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

SwapChain::~SwapChain() {
    Shutdown();
}

bool SwapChain::Initialize(Device& device, HWND hwnd, uint32_t width, uint32_t height) {
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = m_format;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    // Flip-discard is the modern DXGI presentation model for D3D12.
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain1;
    if (!CheckHRESULT(
            device.GetFactory()->CreateSwapChainForHwnd(
                device.GetCommandQueue(),
                hwnd,
                &swapChainDesc,
                nullptr,
                nullptr,
                &swapChain1),
            "CreateSwapChainForHwnd")) {
        return false;
    }

    // Prevent DXGI from handling Alt+Enter fullscreen; GLFW owns the window.
    if (!CheckHRESULT(
            device.GetFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER),
            "MakeWindowAssociation")) {
        return false;
    }

    ComPtr<IDXGISwapChain3> swapChain3;
    if (!CheckHRESULT(swapChain1.As(&swapChain3), "QueryInterface IDXGISwapChain3")) {
        return false;
    }

    swapChain3->AddRef();
    m_swapChain = swapChain3.Get();

    if (!CreateRenderTargets(device)) {
        return false;
    }

    spdlog::info("Swap chain created ({}x{})", width, height);
    return true;
}

void SwapChain::Shutdown() {
    for (auto& renderTarget : m_renderTargets) {
        renderTarget.Reset();
    }

    if (m_rtvHeap) {
        m_rtvHeap->Release();
        m_rtvHeap = nullptr;
    }

    if (m_swapChain) {
        m_swapChain->Release();
        m_swapChain = nullptr;
    }

    m_hwnd = nullptr;
}

bool SwapChain::Resize(Device& device, uint32_t width, uint32_t height) {
    if (!m_swapChain) {
        return false;
    }

    if (width == 0 || height == 0) {
        return true;
    }

    if (width == m_width && height == m_height) {
        return true;
    }

    // Release references to the old back buffers before ResizeBuffers.
    for (auto& renderTarget : m_renderTargets) {
        renderTarget.Reset();
    }

    if (!CheckHRESULT(
            m_swapChain->ResizeBuffers(
                FrameCount,
                width,
                height,
                m_format,
                DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH),
            "ResizeBuffers")) {
        return false;
    }

    m_width = width;
    m_height = height;

    if (!CreateRenderTargets(device)) {
        return false;
    }

    spdlog::info("Swap chain resized ({}x{})", width, height);
    return true;
}

bool SwapChain::CreateRenderTargets(Device& device) {
    if (m_rtvHeap) {
        m_rtvHeap->Release();
        m_rtvHeap = nullptr;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    if (!CheckHRESULT(
            device.GetD3D12Device()->CreateDescriptorHeap(
                &rtvHeapDesc,
                IID_PPV_ARGS(&rtvHeap)),
            "CreateDescriptorHeap RTV")) {
        return false;
    }

    rtvHeap->AddRef();
    m_rtvHeap = rtvHeap.Get();

    m_rtvDescriptorSize = device.GetD3D12Device()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (uint32_t index = 0; index < FrameCount; ++index) {
        ComPtr<ID3D12Resource> renderTarget;
        if (!CheckHRESULT(
                m_swapChain->GetBuffer(index, IID_PPV_ARGS(&renderTarget)),
                "GetBuffer")) {
            return false;
        }

        device.GetD3D12Device()->CreateRenderTargetView(
            renderTarget.Get(),
            nullptr,
            rtvHandle);

        m_renderTargets[index] = renderTarget;
        rtvHandle.ptr += static_cast<SIZE_T>(m_rtvDescriptorSize);
    }

    return true;
}

void SwapChain::Present() {
    if (!m_swapChain) {
        return;
    }
    // Sync interval 1 = vsync on. Change to 0 for uncapped framerate during profiling.
    CheckHRESULT(m_swapChain->Present(1, 0), "Present");
}

uint32_t SwapChain::GetCurrentBackBufferIndex() const {
    if (!m_swapChain) {
        return 0;
    }
    return m_swapChain->GetCurrentBackBufferIndex();
}

ID3D12Resource* SwapChain::GetBackBuffer(uint32_t index) const {
    if (index >= FrameCount) {
        return nullptr;
    }
    return m_renderTargets[index].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetRTV(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * static_cast<SIZE_T>(m_rtvDescriptorSize);
    return handle;
}

} // namespace summer::gfx
