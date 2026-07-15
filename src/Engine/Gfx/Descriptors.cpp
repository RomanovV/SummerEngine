#include "Gfx/Descriptors.h"

namespace summer::gfx {

// --------------------------------------------------------------------------------------------
// SRVDescriptor
// --------------------------------------------------------------------------------------------
SRVDescriptor::~SRVDescriptor() {
    Release();
}

bool SRVDescriptor::CreateTexture2D(
    ID3D12Device* device,
    GpuDescriptorHeap& heap,
    ID3D12Resource* resource,
    DXGI_FORMAT viewFormat,
    uint32_t mipLevels) {
    Release();
    if (!device || !resource) {
        return false;
    }

    m_handle = heap.Allocate();
    if (!m_handle.IsValid()) {
        return false;
    }
    m_heap = &heap;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = viewFormat;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2D.MipLevels = mipLevels;
    device->CreateShaderResourceView(resource, &desc, m_handle.cpu);
    return true;
}

void SRVDescriptor::Release() {
    if (m_heap && m_handle.IsValid()) {
        m_heap->Free(m_handle);
    }
    m_handle = {};
    m_heap = nullptr;
}

// --------------------------------------------------------------------------------------------
// RTVDescriptor
// --------------------------------------------------------------------------------------------
RTVDescriptor::~RTVDescriptor() {
    Release();
}

bool RTVDescriptor::Create(
    ID3D12Device* device,
    CpuDescriptorHeap& heap,
    ID3D12Resource* resource,
    DXGI_FORMAT viewFormat) {
    Release();
    if (!device || !resource) {
        return false;
    }

    m_index = heap.Allocate();
    if (m_index == CpuDescriptorHeap::kInvalidIndex) {
        return false;
    }
    m_heap = &heap;
    m_cpu = heap.HandleForIndex(m_index);

    D3D12_RENDER_TARGET_VIEW_DESC desc{};
    desc.Format = viewFormat;
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(resource, &desc, m_cpu);
    return true;
}

void RTVDescriptor::Release() {
    if (m_heap && m_index != CpuDescriptorHeap::kInvalidIndex) {
        m_heap->Free(m_index);
    }
    m_index = CpuDescriptorHeap::kInvalidIndex;
    m_cpu = {};
    m_heap = nullptr;
}

// --------------------------------------------------------------------------------------------
// DSVDescriptor
// --------------------------------------------------------------------------------------------
DSVDescriptor::~DSVDescriptor() {
    Release();
}

bool DSVDescriptor::Create(
    ID3D12Device* device,
    CpuDescriptorHeap& heap,
    ID3D12Resource* resource,
    DXGI_FORMAT viewFormat) {
    Release();
    if (!device || !resource) {
        return false;
    }

    m_index = heap.Allocate();
    if (m_index == CpuDescriptorHeap::kInvalidIndex) {
        return false;
    }
    m_heap = &heap;
    m_cpu = heap.HandleForIndex(m_index);

    D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
    desc.Format = viewFormat;
    desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    desc.Flags = D3D12_DSV_FLAG_NONE;
    device->CreateDepthStencilView(resource, &desc, m_cpu);
    return true;
}

void DSVDescriptor::Release() {
    if (m_heap && m_index != CpuDescriptorHeap::kInvalidIndex) {
        m_heap->Free(m_index);
    }
    m_index = CpuDescriptorHeap::kInvalidIndex;
    m_cpu = {};
    m_heap = nullptr;
}

} // namespace summer::gfx
