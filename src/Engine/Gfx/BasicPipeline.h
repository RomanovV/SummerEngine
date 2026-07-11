#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

class Device;

// Per-frame constants for the basic mesh shader. Must remain 256-byte aligned for CBV rules.
struct alignas(256) FrameConstants {
    DirectX::XMFLOAT4X4 mvp;
    DirectX::XMFLOAT4 meshColor;
};

// Loads compiled DXIL (.cso), builds root signature + PSO for the basic mesh pass.
class BasicPipeline {
public:
    BasicPipeline() = default;
    ~BasicPipeline();

    BasicPipeline(const BasicPipeline&) = delete;
    BasicPipeline& operator=(const BasicPipeline&) = delete;

    bool Initialize(
        Device& device,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        const char* vertexShaderPath,
        const char* pixelShaderPath);
    void Shutdown();

    void Bind(ID3D12GraphicsCommandList* commandList);
    void UpdateConstants(
        ID3D12GraphicsCommandList* commandList,
        const DirectX::XMFLOAT4X4& mvp,
        const DirectX::XMFLOAT4& meshColor);

    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature; }

private:
    bool LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode);
    bool CreateRootSignature(Device& device);
    bool CreatePipelineState(
        Device& device,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        const std::vector<uint8_t>& vsBytecode,
        const std::vector<uint8_t>& psBytecode);

    ID3D12RootSignature* m_rootSignature = nullptr;
    ID3D12PipelineState* m_pipelineState = nullptr;

    // Persistently-mapped upload buffer for per-frame constant updates.
    ID3D12Resource* m_constantBuffer = nullptr;
    uint8_t* m_constantBufferMapped = nullptr;
};

} // namespace summer::gfx
