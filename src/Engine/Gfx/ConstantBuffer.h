#pragma once

#include <cstdint>
#include <d3d12.h>

namespace summer::gfx {

class Device;

// A persistently-mapped upload buffer holding `count` 256-byte-aligned slots of the same
// element type. Typically one slot per frame-in-flight so the CPU can update the next frame's
// constants while the GPU still reads the previous frame's slot.
class ConstantBuffer {
public:
    ConstantBuffer() = default;
    ~ConstantBuffer();

    ConstantBuffer(const ConstantBuffer&) = delete;
    ConstantBuffer& operator=(const ConstantBuffer&) = delete;

    bool Initialize(Device& device, uint32_t elementSize, uint32_t count);
    void Shutdown();

    // Copies `size` bytes into slot `index`.
    void Update(uint32_t index, const void* data, uint32_t size);

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(uint32_t index) const;

private:
    ID3D12Resource* m_resource = nullptr;
    uint8_t* m_mapped = nullptr;
    uint32_t m_alignedSize = 0;
    uint32_t m_count = 0;
};

} // namespace summer::gfx
