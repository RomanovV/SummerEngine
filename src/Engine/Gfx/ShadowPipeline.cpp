#include "Gfx/ShadowPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <fstream>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

namespace {

// Only POSITION is read. The vertex stride comes from the vertex buffer view (GpuMesh::Bind),
// so a layout that is a subset of the vertex is fine.
constexpr D3D12_INPUT_ELEMENT_DESC kShadowInputLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

} // namespace

ShadowPipeline::~ShadowPipeline() {
    Shutdown();
}

void ShadowPipeline::Shutdown() {
    if (m_pipelineState) {
        m_pipelineState->Release();
        m_pipelineState = nullptr;
    }
    if (m_rootSignature) {
        m_rootSignature->Release();
        m_rootSignature = nullptr;
    }
}

bool ShadowPipeline::LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        spdlog::error("Failed to open shader bytecode file '{}'", path);
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        spdlog::error("Shader bytecode file '{}' is empty", path);
        return false;
    }
    outBytecode.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(outBytecode.data()), size);
    return true;
}

bool ShadowPipeline::CreateRootSignature(Device& device) {
    // Params: [0]=model matrix b0, [1]=light view-projection b1.
    D3D12_ROOT_PARAMETER1 rootParameters[2]{};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Constants.RegisterSpace = 0;
    rootParameters[0].Constants.ShaderRegister = 0; // b0
    rootParameters[0].Constants.Num32BitValues = 16;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Constants.RegisterSpace = 0;
    rootParameters[1].Constants.ShaderRegister = 1; // b1
    rootParameters[1].Constants.Num32BitValues = 16;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootDesc.Desc_1_1.NumParameters = _countof(rootParameters);
    rootDesc.Desc_1_1.pParameters = rootParameters;
    rootDesc.Desc_1_1.NumStaticSamplers = 0;
    rootDesc.Desc_1_1.pStaticSamplers = nullptr;
    rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                              D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (!CheckHRESULT(
            D3D12SerializeVersionedRootSignature(&rootDesc, &signature, &error),
            "D3D12SerializeVersionedRootSignature (shadow)")) {
        if (error) {
            spdlog::error("Root signature error: {}", static_cast<const char*>(error->GetBufferPointer()));
        }
        return false;
    }

    return CheckHRESULT(
        device.GetD3D12Device()->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)),
        "CreateRootSignature (shadow)");
}

bool ShadowPipeline::CreatePipelineState(Device& device, const std::vector<uint8_t>& vsBytecode) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = m_rootSignature;
    psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
    // No pixel shader: depth-only.
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    // Baked into the PSO; tune against shadow acne vs peter-panning.
    psoDesc.RasterizerState.DepthBias = 1000;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 2.0f;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = {kShadowInputLayout, _countof(kShadowInputLayout)};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 0;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    return CheckHRESULT(
        device.GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)),
        "CreateGraphicsPipelineState (shadow)");
}

bool ShadowPipeline::Initialize(Device& device, const char* vertexShaderPath) {
    Shutdown();

    std::vector<uint8_t> vsBytecode;
    if (!LoadShaderBytecode(vertexShaderPath, vsBytecode)) {
        return false;
    }
    if (!CreateRootSignature(device)) {
        return false;
    }
    if (!CreatePipelineState(device, vsBytecode)) {
        Shutdown();
        return false;
    }

    spdlog::info("ShadowPipeline initialized");
    return true;
}

void ShadowPipeline::Bind(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(m_pipelineState);
    commandList->SetGraphicsRootSignature(m_rootSignature);
}

void ShadowPipeline::SetObjectConstants(
    ID3D12GraphicsCommandList* commandList,
    const DirectX::XMFLOAT4X4& model) {
    commandList->SetGraphicsRoot32BitConstants(0, 16, &model, 0);
}

void ShadowPipeline::SetLightViewProj(
    ID3D12GraphicsCommandList* commandList,
    const DirectX::XMFLOAT4X4& lightViewProj) {
    commandList->SetGraphicsRoot32BitConstants(1, 16, &lightViewProj, 0);
}

} // namespace summer::gfx
