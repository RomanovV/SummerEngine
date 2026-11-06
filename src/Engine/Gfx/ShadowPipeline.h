#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

class Device;

// Shadow pass: depth-only, rendered from the light's point of view into the shadow map.
// No pixel shader and no color targets. Bindings:
//   b0  32-bit root constants  - per-object model matrix (16 DWORDs, VS)
//   b1  32-bit root constants  - light view-projection matrix (16 DWORDs, VS)
// Both matrices are expected transposed, same as the rest of the engine's uploads.
class ShadowPipeline {
public:
    ShadowPipeline() = default;
    ~ShadowPipeline();

    ShadowPipeline(const ShadowPipeline&) = delete;
    ShadowPipeline& operator=(const ShadowPipeline&) = delete;

    // Vertex shader is ShadowVS. The PSO writes only DXGI_FORMAT_D32_FLOAT depth.
    bool Initialize(Device& device, const char* vertexShaderPath);
    void Shutdown();

    void Bind(ID3D12GraphicsCommandList* commandList);

    void SetObjectConstants(
        ID3D12GraphicsCommandList* commandList,
        const DirectX::XMFLOAT4X4& model);

    void SetLightViewProj(
        ID3D12GraphicsCommandList* commandList,
        const DirectX::XMFLOAT4X4& lightViewProj);

private:
    bool CreateRootSignature(Device& device);
    bool CreatePipelineState(Device& device, const std::vector<uint8_t>& vsBytecode);
    static bool LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode);

    ID3D12RootSignature* m_rootSignature = nullptr;
    ID3D12PipelineState* m_pipelineState = nullptr;
};

} // namespace summer::gfx
