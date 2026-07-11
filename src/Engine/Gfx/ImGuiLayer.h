#pragma once

#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Platform/Window.h"

#include <d3d12.h>
#include <vector>

struct ImDrawData;
struct ImGui_ImplDX12_InitInfo;

namespace summer::gfx {

// Manages Dear ImGui context and the GLFW + DX12 backends.
// Owns the shared CBV/SRV/UAV descriptor heap used by ImGui (and eventually engine textures).
class ImGuiLayer {
public:
    // Leave headroom beyond ImGui's font atlas for engine texture SRVs later.
    static constexpr uint32_t SrvHeapCapacity = 64;

    ImGuiLayer() = default;
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    bool Initialize(Window& window, Device& device, SwapChain& swapChain);
    void Shutdown();

    // Call once per frame before building any ImGui widgets.
    void BeginFrame();

    // Records ImGui draw commands into an open command list.
    // The back buffer must already be in D3D12_RESOURCE_STATE_RENDER_TARGET.
    void RenderDrawData(ID3D12GraphicsCommandList* commandList);

    ID3D12DescriptorHeap* GetSrvHeap() const { return m_srvHeap; }

private:
    struct DescriptorAllocator {
        void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap);
        void Destroy();
        void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu);
        void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu);

        ID3D12DescriptorHeap* heap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE heapStartCpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE heapStartGpu{};
        UINT heapHandleIncrement = 0;
        std::vector<int> freeIndices;
    };

    static void AllocSrvDescriptor(
        ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
        D3D12_GPU_DESCRIPTOR_HANDLE* outGpu);
    static void FreeSrvDescriptor(
        ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE gpu);

    bool CreateSrvHeap(ID3D12Device* device);

    ID3D12DescriptorHeap* m_srvHeap = nullptr;
    DescriptorAllocator m_srvAllocator;
    bool m_initialized = false;
};

} // namespace summer::gfx
