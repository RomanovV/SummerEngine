#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <spdlog/spdlog.h>

#include <cwchar>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

Device::~Device() {
    Shutdown();
}

bool Device::Initialize(bool enableDebugLayer) {
#if defined(SUMMER_DEBUG)
    if (enableDebugLayer) {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            spdlog::info("D3D12 debug layer enabled");
        }
    }
#else
    (void)enableDebugLayer;
#endif

    uint32_t dxgiFactoryFlags = 0;
#if defined(SUMMER_DEBUG)
    if (enableDebugLayer) {
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    if (!CheckHRESULT(
            CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)),
            "CreateDXGIFactory2")) {
        return false;
    }

    // Pick the first hardware adapter that can create a D3D12 device.
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    for (UINT adapterIndex = 0;
         factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device)))) {
            char adapterName[128]{};
            std::wcstombs(adapterName, desc.Description, sizeof(adapterName) - 1);
            spdlog::info("Using adapter: {}", adapterName);
            break;
        }
    }

    if (!device) {
        if (!CheckHRESULT(
                D3D12CreateDevice(
                    nullptr,
                    D3D_FEATURE_LEVEL_11_0,
                    IID_PPV_ARGS(&device)),
                "D3D12CreateDevice (WARP)")) {
            return false;
        }
        spdlog::warn("Falling back to WARP adapter");
    }

    ComPtr<ID3D12CommandQueue> commandQueue;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (!CheckHRESULT(
            device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)),
            "CreateCommandQueue")) {
        return false;
    }

    // One command allocator per back buffer so we can record frames in parallel with GPU execution.
    for (uint32_t frameIndex = 0; frameIndex < FrameCount; ++frameIndex) {
        ComPtr<ID3D12CommandAllocator> allocator;
        if (!CheckHRESULT(
                device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&allocator)),
                "CreateCommandAllocator")) {
            return false;
        }
        allocator->AddRef();
        m_frames[frameIndex].CommandAllocator = allocator.Get();
    }

    // Create in the recording state, then immediately close -- standard D3D12 init pattern.
    ComPtr<ID3D12GraphicsCommandList> commandList;
    if (!CheckHRESULT(
            device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                m_frames[0].CommandAllocator,
                nullptr,
                IID_PPV_ARGS(&commandList)),
            "CreateCommandList")) {
        return false;
    }
    if (!CheckHRESULT(commandList->Close(), "Close initial command list")) {
        return false;
    }

    ComPtr<ID3D12Fence> fence;
    if (!CheckHRESULT(
            device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
            "CreateFence")) {
        return false;
    }

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        spdlog::error("Failed to create fence event");
        return false;
    }

    // Transfer ComPtr ownership to raw pointers (released in Shutdown).
    factory->AddRef();
    device->AddRef();
    commandQueue->AddRef();
    commandList->AddRef();
    fence->AddRef();

    m_factory = factory.Get();
    m_device = device.Get();
    m_commandQueue = commandQueue.Get();
    m_commandList = commandList.Get();
    m_fence = fence.Get();

    spdlog::info("D3D12 device initialized");
    return true;
}

void Device::Shutdown() {
    WaitForGpu();

    if (m_commandList) {
        m_commandList->Release();
        m_commandList = nullptr;
    }

    for (auto& frame : m_frames) {
        if (frame.CommandAllocator) {
            frame.CommandAllocator->Release();
            frame.CommandAllocator = nullptr;
        }
    }

    if (m_fence) {
        m_fence->Release();
        m_fence = nullptr;
    }

    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    if (m_commandQueue) {
        m_commandQueue->Release();
        m_commandQueue = nullptr;
    }

    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }

    if (m_factory) {
        m_factory->Release();
        m_factory = nullptr;
    }
}

void Device::WaitForGpu() {
    if (!m_commandQueue || !m_fence || !m_fenceEvent) {
        return;
    }

    const uint64_t fenceValue = ++m_fenceCounter;
    if (!CheckHRESULT(
            m_commandQueue->Signal(m_fence, fenceValue),
            "Signal fence during GPU wait")) {
        return;
    }

    if (m_fence->GetCompletedValue() < fenceValue) {
        if (!CheckHRESULT(
                m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent),
                "SetEventOnCompletion")) {
            return;
        }
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Reset per-buffer tracking so a subsequent resize starts from a clean slate.
    for (uint64_t& value : m_fenceValues) {
        value = 0;
    }
}

void Device::WaitForBackBuffer(uint32_t backBufferIndex) {
    if (!m_fence || !m_fenceEvent || backBufferIndex >= FrameCount) {
        return;
    }

    const uint64_t fenceValue = m_fenceValues[backBufferIndex];
    if (m_fence->GetCompletedValue() < fenceValue) {
        if (!CheckHRESULT(
                m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent),
                "SetEventOnCompletion for back buffer")) {
            return;
        }
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void Device::SignalBackBuffer(uint32_t backBufferIndex) {
    if (!m_commandQueue || !m_fence || backBufferIndex >= FrameCount) {
        return;
    }

    const uint64_t fenceValue = ++m_fenceCounter;
    if (!CheckHRESULT(
            m_commandQueue->Signal(m_fence, fenceValue),
            "Signal fence for back buffer")) {
        return;
    }
    m_fenceValues[backBufferIndex] = fenceValue;
}

} // namespace summer::gfx
