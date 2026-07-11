#pragma once

#include "Gfx/Device.h"
#include "Gfx/GpuDescriptorHeap.h"
#include "Gfx/SwapChain.h"
#include "Platform/Window.h"

#include <d3d12.h>

struct ImDrawData;
struct ImGui_ImplDX12_InitInfo;

namespace summer::gfx {

// Manages Dear ImGui context and the GLFW + DX12 backends.
// Borrows a shared shader-visible descriptor heap (owned by the Renderer) for its font atlas
// and any texture previews; engine textures allocate from the same heap.
class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    bool Initialize(Window& window, Device& device, SwapChain& swapChain, GpuDescriptorHeap& srvHeap);
    void Shutdown();

    // Call once per frame before building any ImGui widgets.
    void BeginFrame();

    // Records ImGui draw commands into an open command list.
    // The back buffer must already be in D3D12_RESOURCE_STATE_RENDER_TARGET.
    void RenderDrawData(ID3D12GraphicsCommandList* commandList);

private:
    static void AllocSrvDescriptor(
        ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
        D3D12_GPU_DESCRIPTOR_HANDLE* outGpu);
    static void FreeSrvDescriptor(
        ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE gpu);

    GpuDescriptorHeap* m_srvHeap = nullptr;
    bool m_initialized = false;
};

} // namespace summer::gfx
