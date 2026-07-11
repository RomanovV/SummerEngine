#include "Gfx/Texture2D.h"
#include "Assets/Image.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <vector>

namespace summer::gfx {

Texture2D::~Texture2D() {
    Shutdown();
}

void Texture2D::Shutdown() {
    if (m_srvHeap && m_srv.IsValid()) {
        m_srvHeap->Free(m_srv);
    }
    m_srv = {};
    m_srvHeap = nullptr;

    if (m_resource) {
        m_resource->Release();
        m_resource = nullptr;
    }
    m_width = 0;
    m_height = 0;
    m_mipLevels = 0;
}

bool Texture2D::CreateFromFile(
    Device& device,
    GpuDescriptorHeap& srvHeap,
    const std::string& path,
    bool srgb,
    bool generateMips) {
    summer::assets::Image image;
    if (!summer::assets::Image::LoadFromFile(path, image)) {
        return false;
    }
    return CreateFromImage(device, srvHeap, image, srgb, generateMips);
}

bool Texture2D::CreateFromImage(
    Device& device,
    GpuDescriptorHeap& srvHeap,
    const summer::assets::Image& image,
    bool srgb,
    bool generateMips) {
    if (!image.IsValid()) {
        spdlog::error("Texture2D::CreateFromImage called with invalid image");
        return false;
    }
    if (m_resource) {
        spdlog::error("Texture2D::CreateFromImage called on an already-initialized texture");
        return false;
    }

    ID3D12Device* d3dDevice = device.GetD3D12Device();

    std::vector<summer::assets::Image> mips =
        generateMips ? summer::assets::Image::GenerateMipChain(image)
                     : std::vector<summer::assets::Image>{image};
    m_mipLevels = static_cast<uint32_t>(mips.size());
    m_width = image.width;
    m_height = image.height;

    const DXGI_FORMAT format =
        srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = m_width;
    textureDesc.Height = m_height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = static_cast<UINT16>(m_mipLevels);
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    // Textures (unlike buffers) may legitimately be created in COPY_DEST.
    if (!CheckHRESULT(
            d3dDevice->CreateCommittedResource(
                &defaultHeap,
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_resource)),
            "CreateCommittedResource texture")) {
        return false;
    }

    // Query the padded (256-byte row aligned) layout the copy engine expects for each mip.
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(m_mipLevels);
    std::vector<UINT> rowCounts(m_mipLevels);
    std::vector<UINT64> rowSizes(m_mipLevels);
    UINT64 totalBytes = 0;
    d3dDevice->GetCopyableFootprints(
        &textureDesc, 0, m_mipLevels, 0,
        footprints.data(), rowCounts.data(), rowSizes.data(), &totalBytes);

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = totalBytes;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* uploadBuffer = nullptr;
    if (!CheckHRESULT(
            d3dDevice->CreateCommittedResource(
                &uploadHeap,
                D3D12_HEAP_FLAG_NONE,
                &uploadDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&uploadBuffer)),
            "CreateCommittedResource texture upload")) {
        Shutdown();
        return false;
    }

    uint8_t* mapped = nullptr;
    if (!CheckHRESULT(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped)), "Map texture upload")) {
        uploadBuffer->Release();
        Shutdown();
        return false;
    }

    for (uint32_t mip = 0; mip < m_mipLevels; ++mip) {
        const summer::assets::Image& level = mips[mip];
        const UINT64 destPitch = footprints[mip].Footprint.RowPitch;
        const UINT64 srcPitch = level.RowPitch();
        uint8_t* dstBase = mapped + footprints[mip].Offset;

        for (UINT row = 0; row < rowCounts[mip]; ++row) {
            std::memcpy(
                dstBase + row * destPitch,
                level.pixels.data() + row * srcPitch,
                static_cast<size_t>(rowSizes[mip]));
        }
    }
    uploadBuffer->Unmap(0, nullptr);

    // Record the copy on the device's command list, then execute and wait (one-shot upload).
    FrameContext& frame = device.GetFrame(0);
    ID3D12GraphicsCommandList* commandList = device.GetCommandList();
    if (!CheckHRESULT(frame.CommandAllocator->Reset(), "Reset allocator for texture upload") ||
        !CheckHRESULT(commandList->Reset(frame.CommandAllocator, nullptr), "Reset command list for texture upload")) {
        uploadBuffer->Release();
        Shutdown();
        return false;
    }

    for (uint32_t mip = 0; mip < m_mipLevels; ++mip) {
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = m_resource;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = mip;

        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = uploadBuffer;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprints[mip];

        commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_resource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    if (!CheckHRESULT(commandList->Close(), "Close command list for texture upload")) {
        uploadBuffer->Release();
        Shutdown();
        return false;
    }

    ID3D12CommandList* lists[] = {commandList};
    device.GetCommandQueue()->ExecuteCommandLists(1, lists);
    device.WaitForGpu();

    uploadBuffer->Release();

    // Allocate and populate the SRV in the shared heap.
    m_srvHeap = &srvHeap;
    m_srv = srvHeap.Allocate();
    if (!m_srv.IsValid()) {
        Shutdown();
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = m_mipLevels;
    d3dDevice->CreateShaderResourceView(m_resource, &srvDesc, m_srv.cpu);

    spdlog::info("Texture2D created ({}x{}, {} mips, {})",
        m_width, m_height, m_mipLevels, srgb ? "sRGB" : "linear");
    return true;
}

} // namespace summer::gfx
