#include "Gfx/GBuffer.h"

#include <spdlog/spdlog.h>

namespace summer::gfx {

namespace {

struct ColorSlotDesc {
    DXGI_FORMAT format;
    float clear[4];
};

const ColorSlotDesc kSlots[GBuffer::ColorCount] = {
    {GBuffer::kAlbedoFormat, {0.0f, 0.0f, 0.0f, 1.0f}},     // albedo 0, occlusion 1
    {GBuffer::kNormalFormat, {0.0f, 0.0f, 1.0f, 0.0f}},     // world +Z
    {GBuffer::kMaterialFormat, {0.0f, 1.0f, 0.0f, 1.0f}},   // metal 0, rough 1, emissive 0
};

} // namespace

GBuffer::~GBuffer() {
    Shutdown();
}

bool GBuffer::Initialize(
    ID3D12Device* device,
    CpuDescriptorHeap& rtvHeap,
    CpuDescriptorHeap& dsvHeap,
    GpuDescriptorHeap& srvHeap,
    uint32_t width,
    uint32_t height) {
    Shutdown();

    if (!device || width == 0 || height == 0) {
        return false;
    }

    m_width = width;
    m_height = height;
    m_colors.reserve(ColorCount);
    for (uint32_t slot = 0; slot < ColorCount; ++slot) {
        m_colors.emplace_back();
        if (!m_colors.back().Initialize(
                device,
                rtvHeap,
                srvHeap,
                width,
                height,
                kSlots[slot].format,
                kSlots[slot].clear)) {
            Shutdown();
            return false;
        }
    }

    if (!m_depth.Initialize(
            device, dsvHeap, width, height, kDepthFormat, &srvHeap)) {
        Shutdown();
        return false;
    }

    spdlog::info(
        "GBuffer created ({}x{}, {} color targets, sampleable depth)",
        width,
        height,
        static_cast<uint32_t>(ColorCount));
    return true;
}

void GBuffer::Shutdown() {
    m_depth.Shutdown();
    m_colors.clear();
    m_width = 0;
    m_height = 0;
}

bool GBuffer::Resize(ID3D12Device* device, uint32_t width, uint32_t height) {
    if (m_colors.size() != ColorCount || width == 0 || height == 0) {
        return false;
    }
    if (width == m_width && height == m_height) {
        return true;
    }

    for (RenderTarget& target : m_colors) {
        if (!target.Resize(device, width, height)) {
            return false;
        }
    }
    if (!m_depth.Resize(device, width, height)) {
        return false;
    }

    m_width = width;
    m_height = height;
    return true;
}

void GBuffer::CopyColorRtvs(D3D12_CPU_DESCRIPTOR_HANDLE* out) const {
    for (uint32_t slot = 0; slot < m_colors.size(); ++slot) {
        out[slot] = m_colors[slot].RTV();
    }
}

void GBuffer::TransitionColorsTo(
    ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state) {
    for (RenderTarget& target : m_colors) {
        target.TransitionTo(commandList, state);
    }
}

void GBuffer::TransitionDepthTo(
    ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state) {
    m_depth.TransitionTo(commandList, state);
}

void GBuffer::Clear(ID3D12GraphicsCommandList* commandList) const {
    for (const RenderTarget& target : m_colors) {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = target.RTV();
        commandList->ClearRenderTargetView(rtv, target.ClearColor(), 0, nullptr);
    }
    commandList->ClearDepthStencilView(
        m_depth.DSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

} // namespace summer::gfx
