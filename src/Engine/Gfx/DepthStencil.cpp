#include "Gfx/DepthStencil.h"
#include "Gfx/CpuDescriptorHeap.h"
#include "Gfx/GpuDescriptorHeap.h"

#include <spdlog/spdlog.h>
#include <utility>

namespace summer::gfx {

namespace {

// A depth resource that must also be sampled has to be created typeless: the DSV binds the depth
// view format, the SRV binds the "red channel" color view of the same bits.
struct DepthFormatTriple {
    DXGI_FORMAT resource; // what the ID3D12Resource is created with
    DXGI_FORMAT dsv;      // depth-stencil view format
    DXGI_FORMAT srv;      // shader resource view format (UNKNOWN if not sampleable)
};

DepthFormatTriple MapDepthFormats(DXGI_FORMAT depthFormat, bool needSrv) {
    if (!needSrv) {
        // Not sampled: create directly with the depth format, no SRV.
        return {depthFormat, depthFormat, DXGI_FORMAT_UNKNOWN};
    }

    switch (depthFormat) {
        case DXGI_FORMAT_D32_FLOAT:
            return {DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT};
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return {DXGI_FORMAT_R24G8_TYPELESS,
                    DXGI_FORMAT_D24_UNORM_S8_UINT,
                    DXGI_FORMAT_R24_UNORM_X8_TYPELESS};
        case DXGI_FORMAT_D16_UNORM:
            return {DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_R16_UNORM};
        default:
            spdlog::warn("DepthStencil: unmapped sampleable depth format {}; SRV disabled",
                         static_cast<int>(depthFormat));
            return {depthFormat, depthFormat, DXGI_FORMAT_UNKNOWN};
    }
}

} // namespace

DepthStencil::~DepthStencil() {
    Shutdown();
}

DepthStencil::DepthStencil(DepthStencil&& other) noexcept
    : m_dsvHeap(other.m_dsvHeap),
      m_srvHeap(other.m_srvHeap),
      m_texture(std::move(other.m_texture)),
      m_dsv(std::move(other.m_dsv)),
      m_srv(std::move(other.m_srv)),
      m_dsvFormat(other.m_dsvFormat),
      m_width(other.m_width),
      m_height(other.m_height) {
    other.m_dsvHeap = nullptr;
    other.m_srvHeap = nullptr;
    other.m_width = 0;
    other.m_height = 0;
}

DepthStencil& DepthStencil::operator=(DepthStencil&& other) noexcept {
    if (this != &other) {
        Shutdown();
        m_dsvHeap = other.m_dsvHeap;
        m_srvHeap = other.m_srvHeap;
        m_texture = std::move(other.m_texture);
        m_dsv = std::move(other.m_dsv);
        m_srv = std::move(other.m_srv);
        m_dsvFormat = other.m_dsvFormat;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_dsvHeap = nullptr;
        other.m_srvHeap = nullptr;
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

bool DepthStencil::Initialize(
    ID3D12Device* device,
    CpuDescriptorHeap& dsvHeap,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    GpuDescriptorHeap* srvHeap) {
    Shutdown();

    m_dsvHeap = &dsvHeap;
    m_srvHeap = srvHeap;
    m_dsvFormat = format;
    m_width = width;
    m_height = height;

    if (!CreateResourceAndViews(device)) {
        Shutdown();
        return false;
    }

    spdlog::info("DepthStencil created ({}x{}{})", m_width, m_height, srvHeap ? ", sampleable" : "");
    return true;
}

bool DepthStencil::CreateResourceAndViews(ID3D12Device* device) {
    if (m_width == 0 || m_height == 0) {
        return false;
    }

    const bool needSrv = m_srvHeap != nullptr;
    const DepthFormatTriple fmt = MapDepthFormats(m_dsvFormat, needSrv);

    GpuTexture::Desc desc{};
    desc.width = m_width;
    desc.height = m_height;
    desc.format = fmt.resource;
    desc.flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    desc.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    desc.hasClearValue = true;
    desc.clearValue.Format = fmt.dsv;
    desc.clearValue.DepthStencil.Depth = 1.0f;
    desc.clearValue.DepthStencil.Stencil = 0;

    if (!m_texture.Create(device, desc)) {
        return false;
    }
    if (!m_dsv.Create(device, *m_dsvHeap, m_texture.Resource(), fmt.dsv)) {
        return false;
    }
    if (needSrv && fmt.srv != DXGI_FORMAT_UNKNOWN) {
        if (!m_srv.CreateTexture2D(device, *m_srvHeap, m_texture.Resource(), fmt.srv, 1)) {
            return false;
        }
    }
    return true;
}

bool DepthStencil::Resize(ID3D12Device* device, uint32_t width, uint32_t height) {
    if (!m_dsvHeap || width == 0 || height == 0) {
        return false;
    }
    if (width == m_width && height == m_height) {
        return true;
    }

    m_dsv.Release();
    m_srv.Release();
    m_texture.Release();
    m_width = width;
    m_height = height;
    return CreateResourceAndViews(device);
}

void DepthStencil::TransitionTo(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES state) {
    m_texture.Transition(commandList, state);
}

void DepthStencil::Shutdown() {
    m_dsv.Release();
    m_srv.Release();
    m_texture.Release();
    m_dsvHeap = nullptr;
    m_srvHeap = nullptr;
    m_width = 0;
    m_height = 0;
}

} // namespace summer::gfx
