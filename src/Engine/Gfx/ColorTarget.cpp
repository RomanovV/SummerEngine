#include "Gfx/ColorTarget.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <spdlog/spdlog.h>

namespace summer::gfx {

ColorTarget::~ColorTarget() {
    Shutdown();
}

bool ColorTarget::Initialize(
    Device& device,
    GpuDescriptorHeap& srvHeap,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format) {
    Shutdown();

    m_srvHeap = &srvHeap;
    m_format = format;
    m_width = width;
    m_height = height;

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (!CheckHRESULT(
            device.GetD3D12Device()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)),
            "CreateDescriptorHeap (HDR RTV)")) {
        return false;
    }
    m_rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    m_srv = srvHeap.Allocate();
    if (!m_srv.IsValid()) {
        Shutdown();
        return false;
    }

    if (!CreateResourceAndViews(device)) {
        Shutdown();
        return false;
    }

    spdlog::info("ColorTarget (HDR) created ({}x{})", m_width, m_height);
    return true;
}

bool ColorTarget::CreateResourceAndViews(Device& device) {
    if (m_width == 0 || m_height == 0) {
        return false;
    }

    ID3D12Device* d3dDevice = device.GetD3D12Device();

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = m_width;
    desc.Height = m_height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = m_format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = m_format;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

    // Start sampleable; the Renderer transitions to RENDER_TARGET at the top of each frame.
    if (!CheckHRESULT(
            d3dDevice->CreateCommittedResource(
                &defaultHeap,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                &clearValue,
                IID_PPV_ARGS(&m_resource)),
            "CreateCommittedResource (HDR target)")) {
        return false;
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = m_format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    d3dDevice->CreateRenderTargetView(m_resource, &rtvDesc, m_rtv);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = m_format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    d3dDevice->CreateShaderResourceView(m_resource, &srvDesc, m_srv.cpu);

    return true;
}

bool ColorTarget::Resize(Device& device, uint32_t width, uint32_t height) {
    if (!m_srvHeap || width == 0 || height == 0) {
        return false;
    }
    if (width == m_width && height == m_height) {
        return true;
    }

    if (m_resource) {
        m_resource->Release();
        m_resource = nullptr;
    }
    m_width = width;
    m_height = height;
    return CreateResourceAndViews(device);
}

void ColorTarget::Shutdown() {
    if (m_resource) {
        m_resource->Release();
        m_resource = nullptr;
    }
    if (m_rtvHeap) {
        m_rtvHeap->Release();
        m_rtvHeap = nullptr;
    }
    if (m_srvHeap && m_srv.IsValid()) {
        m_srvHeap->Free(m_srv);
    }
    m_srv = {};
    m_srvHeap = nullptr;
    m_width = 0;
    m_height = 0;
}

} // namespace summer::gfx
