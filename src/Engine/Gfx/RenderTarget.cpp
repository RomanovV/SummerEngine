#include "Gfx/RenderTarget.h"
#include "Gfx/CpuDescriptorHeap.h"
#include "Gfx/GpuDescriptorHeap.h"

#include <spdlog/spdlog.h>

namespace summer::gfx {

RenderTarget::~RenderTarget() {
    Shutdown();
}

bool RenderTarget::Initialize(
    ID3D12Device* device,
    CpuDescriptorHeap& rtvHeap,
    GpuDescriptorHeap& srvHeap,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const float clearColor[4]) {
    Shutdown();

    m_rtvHeap = &rtvHeap;
    m_srvHeap = &srvHeap;
    m_format = format;
    m_width = width;
    m_height = height;
    if (clearColor) {
        for (int i = 0; i < 4; ++i) {
            m_clearColor[i] = clearColor[i];
        }
    }

    if (!CreateResourceAndViews(device)) {
        Shutdown();
        return false;
    }

    spdlog::info("RenderTarget created ({}x{})", m_width, m_height);
    return true;
}

bool RenderTarget::CreateResourceAndViews(ID3D12Device* device) {
    if (m_width == 0 || m_height == 0) {
        return false;
    }

    GpuTexture::Desc desc{};
    desc.width = m_width;
    desc.height = m_height;
    desc.format = m_format;
    desc.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    // Start sampleable; the renderer flips to RENDER_TARGET at the top of the pass.
    desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    desc.hasClearValue = true;
    desc.clearValue.Format = m_format;
    desc.clearValue.Color[0] = m_clearColor[0];
    desc.clearValue.Color[1] = m_clearColor[1];
    desc.clearValue.Color[2] = m_clearColor[2];
    desc.clearValue.Color[3] = m_clearColor[3];

    if (!m_texture.Create(device, desc)) {
        return false;
    }
    if (!m_rtv.Create(device, *m_rtvHeap, m_texture.Resource(), m_format)) {
        return false;
    }
    if (!m_srv.CreateTexture2D(device, *m_srvHeap, m_texture.Resource(), m_format, 1)) {
        return false;
    }
    return true;
}

bool RenderTarget::Resize(ID3D12Device* device, uint32_t width, uint32_t height) {
    if (!m_rtvHeap || !m_srvHeap || width == 0 || height == 0) {
        return false;
    }
    if (width == m_width && height == m_height) {
        return true;
    }

    m_rtv.Release();
    m_srv.Release();
    m_texture.Release();
    m_width = width;
    m_height = height;
    return CreateResourceAndViews(device);
}

void RenderTarget::TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state) {
    m_texture.Transition(commandList, state);
}

void RenderTarget::Shutdown() {
    m_rtv.Release();
    m_srv.Release();
    m_texture.Release();
    m_rtvHeap = nullptr;
    m_srvHeap = nullptr;
    m_width = 0;
    m_height = 0;
}

} // namespace summer::gfx
