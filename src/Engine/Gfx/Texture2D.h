#pragma once

#include "Gfx/GpuDescriptorHeap.h"

#include <cstdint>
#include <d3d12.h>
#include <string>

namespace summer::assets {
struct Image;
}

namespace summer::gfx {

class Device;

// A 2D GPU texture: uploads RGBA8 pixel data (with a mip chain) to a DEFAULT-heap resource and
// allocates an SRV in the shared descriptor heap. Bind GetSrvGpu() to a descriptor table.
class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    // Loads an image file and uploads it. srgb selects an _SRGB view format for color textures.
    bool CreateFromFile(
        Device& device,
        GpuDescriptorHeap& srvHeap,
        const std::string& path,
        bool srgb = true,
        bool generateMips = true);

    // Uploads an already-decoded CPU image.
    bool CreateFromImage(
        Device& device,
        GpuDescriptorHeap& srvHeap,
        const summer::assets::Image& image,
        bool srgb = true,
        bool generateMips = true);

    void Shutdown();

    bool IsValid() const { return m_resource != nullptr; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpu() const { return m_srv.gpu; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    uint32_t GetMipLevels() const { return m_mipLevels; }

private:
    GpuDescriptorHeap* m_srvHeap = nullptr;
    GpuDescriptorHeap::Handle m_srv;
    ID3D12Resource* m_resource = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_mipLevels = 0;
};

} // namespace summer::gfx
