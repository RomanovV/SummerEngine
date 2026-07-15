#include "Gfx/TonemapPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <fstream>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

namespace {
struct TonemapConstants {
    float exposure;
    uint32_t op;
    float _pad0;
    float _pad1;
};
} // namespace

TonemapPipeline::~TonemapPipeline() {
    Shutdown();
}

void TonemapPipeline::Shutdown() {
    if (m_pipelineState) {
        m_pipelineState->Release();
        m_pipelineState = nullptr;
    }
    if (m_rootSignature) {
        m_rootSignature->Release();
        m_rootSignature = nullptr;
    }
}

bool TonemapPipeline::LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode) {
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

bool TonemapPipeline::CreateRootSignature(Device& device) {
    D3D12_DESCRIPTOR_RANGE1 srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // t0
    srvRange.RegisterSpace = 0;
    // The HDR target is re-rendered every frame, so its contents change between executions.
    // DATA_STATIC would (incorrectly) promise the driver they never change -> the debug layer's
    // DATA_STATIC_DESCRIPTOR_INVALID_DATA_CHANGE error. DATA_STATIC_WHILE_SET_AT_EXECUTE only
    // guarantees stability from when the table is bound (after the HDR pass writes) to end of
    // execution, which is exactly the render-then-sample pattern here.
    srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER1 rootParameters[2]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &srvRange;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Constants.RegisterSpace = 0;
    rootParameters[1].Constants.ShaderRegister = 0; // b0
    rootParameters[1].Constants.Num32BitValues = sizeof(TonemapConstants) / 4;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0; // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootDesc.Desc_1_1.NumParameters = _countof(rootParameters);
    rootDesc.Desc_1_1.pParameters = rootParameters;
    rootDesc.Desc_1_1.NumStaticSamplers = 1;
    rootDesc.Desc_1_1.pStaticSamplers = &sampler;
    rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (!CheckHRESULT(
            D3D12SerializeVersionedRootSignature(&rootDesc, &signature, &error),
            "D3D12SerializeVersionedRootSignature (tonemap)")) {
        if (error) {
            spdlog::error("Root signature error: {}", static_cast<const char*>(error->GetBufferPointer()));
        }
        return false;
    }

    return CheckHRESULT(
        device.GetD3D12Device()->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)),
        "CreateRootSignature (tonemap)");
}

bool TonemapPipeline::CreatePipelineState(
    Device& device,
    DXGI_FORMAT rtvFormat,
    const std::vector<uint8_t>& vsBytecode,
    const std::vector<uint8_t>& psBytecode) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = m_rootSignature;
    psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
    psoDesc.PS = {psBytecode.data(), psBytecode.size()};
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = {nullptr, 0}; // fullscreen triangle from SV_VertexID
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    psoDesc.SampleDesc.Count = 1;

    return CheckHRESULT(
        device.GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)),
        "CreateGraphicsPipelineState (tonemap)");
}

bool TonemapPipeline::Initialize(
    Device& device,
    DXGI_FORMAT rtvFormat,
    const char* vertexShaderPath,
    const char* pixelShaderPath) {
    Shutdown();

    std::vector<uint8_t> vsBytecode;
    std::vector<uint8_t> psBytecode;
    if (!LoadShaderBytecode(vertexShaderPath, vsBytecode) ||
        !LoadShaderBytecode(pixelShaderPath, psBytecode)) {
        return false;
    }
    if (!CreateRootSignature(device)) {
        return false;
    }
    if (!CreatePipelineState(device, rtvFormat, vsBytecode, psBytecode)) {
        Shutdown();
        return false;
    }

    spdlog::info("TonemapPipeline initialized");
    return true;
}

void TonemapPipeline::Draw(
    ID3D12GraphicsCommandList* commandList,
    D3D12_GPU_DESCRIPTOR_HANDLE hdrSrv,
    float exposure,
    Operator op) {
    commandList->SetPipelineState(m_pipelineState);
    commandList->SetGraphicsRootSignature(m_rootSignature);
    commandList->SetGraphicsRootDescriptorTable(0, hdrSrv);

    TonemapConstants constants{exposure, static_cast<uint32_t>(op), 0.0f, 0.0f};
    commandList->SetGraphicsRoot32BitConstants(1, sizeof(TonemapConstants) / 4, &constants, 0);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 0, nullptr);
    commandList->IASetIndexBuffer(nullptr);
    commandList->DrawInstanced(3, 1, 0, 0);
}

} // namespace summer::gfx
