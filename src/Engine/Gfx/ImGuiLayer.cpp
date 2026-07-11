#include "Gfx/ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_glfw.h>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace summer::gfx {

ImGuiLayer::~ImGuiLayer() {
    Shutdown();
}

void ImGuiLayer::AllocSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
    D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
    auto* layer = static_cast<ImGuiLayer*>(info->UserData);
    layer->m_srvHeap->AllocateRaw(outCpu, outGpu);
}

void ImGuiLayer::FreeSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu,
    D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    auto* layer = static_cast<ImGuiLayer*>(info->UserData);
    layer->m_srvHeap->FreeRaw(cpu, gpu);
}

bool ImGuiLayer::Initialize(
    Window& window,
    Device& device,
    SwapChain& swapChain,
    GpuDescriptorHeap& srvHeap) {
    if (m_initialized) {
        return true;
    }

    m_srvHeap = &srvHeap;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    auto* glfwWindow = static_cast<GLFWwindow*>(window.GetGLFWHandle());
    if (!ImGui_ImplGlfw_InitForOther(glfwWindow, true)) {
        spdlog::error("ImGui_ImplGlfw_InitForOther failed");
        return false;
    }

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = device.GetD3D12Device();
    initInfo.CommandQueue = device.GetCommandQueue();
    initInfo.NumFramesInFlight = FrameCount;
    initInfo.RTVFormat = swapChain.GetFormat();
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = m_srvHeap->Heap();
    initInfo.UserData = this;
    initInfo.SrvDescriptorAllocFn = AllocSrvDescriptor;
    initInfo.SrvDescriptorFreeFn = FreeSrvDescriptor;

    if (!ImGui_ImplDX12_Init(&initInfo)) {
        spdlog::error("ImGui_ImplDX12_Init failed");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_initialized = true;
    spdlog::info("ImGui layer initialized");
    return true;
}

void ImGuiLayer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_srvHeap = nullptr;
    m_initialized = false;
}

void ImGuiLayer::BeginFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::RenderDrawData(ID3D12GraphicsCommandList* commandList) {
    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->TotalVtxCount == 0) {
        return;
    }

    // DX12 allows only one shader-visible heap to be bound per command list.
    ID3D12DescriptorHeap* heaps[] = {m_srvHeap->Heap()};
    commandList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(drawData, commandList);
}

} // namespace summer::gfx
