#pragma once

#include <cstdint>
#include <d3d12.h>

namespace summer::gfx {

class Device;

// Uploads data to a DEFAULT-heap GPU buffer via a one-shot copy on the device command list.
class Buffer {
public:
    Buffer() = default;
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    bool CreateAndUpload(
        Device& device,
        const void* data,
        uint64_t size,
        D3D12_RESOURCE_STATES finalState);

    void Shutdown();

    ID3D12Resource* GetResource() const { return m_resource; }
    uint64_t GetSize() const { return m_size; }

private:
    ID3D12Resource* m_resource = nullptr;
    uint64_t m_size = 0;
};

} // namespace summer::gfx
