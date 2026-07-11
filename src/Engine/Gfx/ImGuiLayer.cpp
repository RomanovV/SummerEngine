#include "Gfx/ImGuiLayer.h"
#include "Gfx/Helpers.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_glfw.h>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

ImGuiLayer::~ImGuiLayer() {
    Shutdown();
}

void ImGuiLayer::DescriptorAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* heap) {
    this->heap = heap;

    const D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
    heapStartCpu = heap->GetCPUDescriptorHandleForHeapStart();
    heapStartGpu = heap->GetGPUDescriptorHandleForHeapStart();
    heapHandleIncrement = device->GetDescriptorHandleIncrementSize(desc.Type);

    freeIndices.reserve(desc.NumDescriptors);
    for (int index = static_cast<int>(desc.NumDescriptors) - 1; index >= 0; --index) {
        freeIndices.push_back(index);
    }
}

void ImGuiLayer::DescriptorAllocator::Destroy() {
    heap = nullptr;
    freeIndices.clear();
}

void ImGuiLayer::DescriptorAllocator::Alloc(
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
    D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
    if (freeIndices.empty()) {
        spdlog::error("ImGui SRV descriptor heap is full");
        return;
    }

    const int index = freeIndices.back();
    freeIndices.pop_back();

    outCpu->ptr = heapStartCpu.ptr + static_cast<SIZE_T>(index) * heapHandleIncrement;
    outGpu->ptr = heapStartGpu.ptr + static_cast<SIZE_T>(index) * heapHandleIncrement;
}

void ImGuiLayer::DescriptorAllocator::Free(
    D3D12_CPU_DESCRIPTOR_HANDLE cpu,
    D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    const int cpuIndex = static_cast<int>((cpu.ptr - heapStartCpu.ptr) / heapHandleIncrement);
    const int gpuIndex = static_cast<int>((gpu.ptr - heapStartGpu.ptr) / heapHandleIncrement);

    if (cpuIndex != gpuIndex) {
        spdlog::warn("ImGui descriptor free mismatch (cpu={}, gpu={})", cpuIndex, gpuIndex);
    }

    freeIndices.push_back(cpuIndex);
}

void ImGuiLayer::AllocSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
    D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
    auto* layer = static_cast<ImGuiLayer*>(info->UserData);
    layer->m_srvAllocator.Alloc(outCpu, outGpu);
}

void ImGuiLayer::FreeSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE cpu,
    D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    auto* layer = static_cast<ImGuiLayer*>(info->UserData);
    layer->m_srvAllocator.Free(cpu, gpu);
}

bool ImGuiLayer::CreateSrvHeap(ID3D12Device* device) {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = SrvHeapCapacity;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    // Shader-visible heap: required so ImGui (and future engine passes) can sample textures.
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> heap;
    if (!CheckHRESULT(
            device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)),
            "CreateDescriptorHeap SRV")) {
        return false;
    }

    heap->AddRef();
    m_srvHeap = heap.Get();
    m_srvAllocator.Create(device, m_srvHeap);
    return true;
}

bool ImGuiLayer::Initialize(Window& window, Device& device, SwapChain& swapChain) {
    if (m_initialized) {
        return true;
    }

    if (!CreateSrvHeap(device.GetD3D12Device())) {
        return false;
    }

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
    initInfo.SrvDescriptorHeap = m_srvHeap;
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

    m_srvAllocator.Destroy();
    if (m_srvHeap) {
        m_srvHeap->Release();
        m_srvHeap = nullptr;
    }

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
    ID3D12DescriptorHeap* heaps[] = {m_srvHeap};
    commandList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(drawData, commandList);
}

} // namespace summer::gfx
