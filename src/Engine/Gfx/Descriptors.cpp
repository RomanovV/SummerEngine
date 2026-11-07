#include "Gfx/Descriptors.h"

namespace summer::gfx {

// --------------------------------------------------------------------------------------------
// SRVDescriptor
// --------------------------------------------------------------------------------------------
SRVDescriptor::~SRVDescriptor() {
    Release();
}

SRVDescriptor::SRVDescriptor(SRVDescriptor&& other) noexcept
    : m_heap(other.m_heap), m_handle(other.m_handle) {
    other.m_heap = nullptr;
    other.m_handle = {};
}

SRVDescriptor& SRVDescriptor::operator=(SRVDescriptor&& other) noexcept {
    if (this != &other) {
        Release();
        m_heap = other.m_heap;
        m_handle = other.m_handle;
        other.m_heap = nullptr;
        other.m_handle = {};
    }
    return *this;
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

bool SRVDescriptor::CreateTexture2DArray(
    ID3D12Device* device,
    GpuDescriptorHeap& heap,
    ID3D12Resource* resource,
    DXGI_FORMAT viewFormat,
    uint32_t firstArraySlice,
    uint32_t arraySize,
    uint32_t mipLevels) {
    Release();
    if (!device || !resource || arraySize == 0 || mipLevels == 0) {
        return false;
    }
    const D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || resourceDesc.SampleDesc.Count != 1 ||
        firstArraySlice >= resourceDesc.DepthOrArraySize ||
        arraySize > resourceDesc.DepthOrArraySize - firstArraySlice || mipLevels > resourceDesc.MipLevels) {
        return false;
    }

    m_handle = heap.Allocate();
    if (!m_handle.IsValid()) {
        return false;
    }
    m_heap = &heap;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = viewFormat;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2DArray.MostDetailedMip = 0;
    desc.Texture2DArray.MipLevels = mipLevels;
    desc.Texture2DArray.FirstArraySlice = firstArraySlice;
    desc.Texture2DArray.ArraySize = arraySize;
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

RTVDescriptor::RTVDescriptor(RTVDescriptor&& other) noexcept
    : m_heap(other.m_heap), m_cpu(other.m_cpu), m_index(other.m_index) {
    other.m_heap = nullptr;
    other.m_cpu = {};
    other.m_index = CpuDescriptorHeap::kInvalidIndex;
}

RTVDescriptor& RTVDescriptor::operator=(RTVDescriptor&& other) noexcept {
    if (this != &other) {
        Release();
        m_heap = other.m_heap;
        m_cpu = other.m_cpu;
        m_index = other.m_index;
        other.m_heap = nullptr;
        other.m_cpu = {};
        other.m_index = CpuDescriptorHeap::kInvalidIndex;
    }
    return *this;
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

DSVDescriptor::DSVDescriptor(DSVDescriptor&& other) noexcept
    : m_heap(other.m_heap), m_cpu(other.m_cpu), m_index(other.m_index) {
    other.m_heap = nullptr;
    other.m_cpu = {};
    other.m_index = CpuDescriptorHeap::kInvalidIndex;
}

DSVDescriptor& DSVDescriptor::operator=(DSVDescriptor&& other) noexcept {
    if (this != &other) {
        Release();
        m_heap = other.m_heap;
        m_cpu = other.m_cpu;
        m_index = other.m_index;
        other.m_heap = nullptr;
        other.m_cpu = {};
        other.m_index = CpuDescriptorHeap::kInvalidIndex;
    }
    return *this;
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

bool DSVDescriptor::CreateTexture2DArraySlice(
    ID3D12Device* device,
    CpuDescriptorHeap& heap,
    ID3D12Resource* resource,
    DXGI_FORMAT viewFormat,
    uint32_t arraySlice) {
    Release();
    if (!device || !resource) {
        return false;
    }
    const D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || resourceDesc.SampleDesc.Count != 1 ||
        arraySlice >= resourceDesc.DepthOrArraySize) {
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
    desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    desc.Flags = D3D12_DSV_FLAG_NONE;
    desc.Texture2DArray.MipSlice = 0;
    desc.Texture2DArray.FirstArraySlice = arraySlice;
    desc.Texture2DArray.ArraySize = 1;
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
