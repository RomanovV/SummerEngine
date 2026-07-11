#pragma once

#include <cstdint>
#include <d3d12.h>

namespace summer::gfx {

class Device;

// Uploads data to a DEFAULT-heap GPU buffer via a one-shot copy on the device command list.
// Upload staging resources must stay alive until the copy command list has finished executing.
class Buffer {
public:
    Buffer() = default;
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Records copy + transition barriers onto device.GetCommandList().
    // Call ReleaseUploadStaging() after ExecuteCommandLists + GPU wait.
    bool CreateAndUpload(
        Device& device,
        const void* data,
        uint64_t size,
        D3D12_RESOURCE_STATES finalState);

    void ReleaseUploadStaging();
    void Shutdown();

    ID3D12Resource* GetResource() const { return m_resource; }
    uint64_t GetSize() const { return m_size; }

private:
    ID3D12Resource* m_resource = nullptr;
    ID3D12Resource* m_uploadStaging = nullptr;
    uint64_t m_size = 0;
};

} // namespace summer::gfx
