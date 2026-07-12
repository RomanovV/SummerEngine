#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

class Device;
struct Material;

// Forward PBR pipeline: metallic-roughness + tangent-space normal mapping, rendering into an
// HDR target. Bindings:
//   b0  32-bit root constants  - per-object (model + normalMatrix)
//   b1  CBV                    - per-frame (camera + lights)
//   b2  32-bit root constants  - per-material factors
//   t0..t4 SRV tables          - baseColor, normal, metalRough, occlusion, emissive
//   s0  static sampler         - anisotropic wrap
class PbrPipeline {
public:
    PbrPipeline() = default;
    ~PbrPipeline();

    PbrPipeline(const PbrPipeline&) = delete;
    PbrPipeline& operator=(const PbrPipeline&) = delete;

    bool Initialize(
        Device& device,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        const char* vertexShaderPath,
        const char* pixelShaderPath);
    void Shutdown();

    void Bind(ID3D12GraphicsCommandList* commandList);

    void SetObjectConstants(
        ID3D12GraphicsCommandList* commandList,
        const DirectX::XMFLOAT4X4& model,
        const DirectX::XMFLOAT4X4& normalMatrix);

    void SetFrameConstants(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_VIRTUAL_ADDRESS frameCbAddress);

    // Sets per-material factors (b2) and binds the five material textures (t0..t4).
    void SetMaterial(ID3D12GraphicsCommandList* commandList, const Material& material);

private:
    bool CreateRootSignature(Device& device);
    bool CreatePipelineState(
        Device& device,
        DXGI_FORMAT rtvFormat,
        DXGI_FORMAT dsvFormat,
        const std::vector<uint8_t>& vsBytecode,
        const std::vector<uint8_t>& psBytecode);
    static bool LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode);

    ID3D12RootSignature* m_rootSignature = nullptr;
    ID3D12PipelineState* m_pipelineState = nullptr;
};

} // namespace summer::gfx
