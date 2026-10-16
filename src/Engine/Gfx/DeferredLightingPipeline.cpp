#include "Gfx/DeferredLightingPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <fstream>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

DeferredLightingPipeline::~DeferredLightingPipeline() {
    Shutdown();
}

void DeferredLightingPipeline::Shutdown() {
    if (m_pipelineState) {
        m_pipelineState->Release();
        m_pipelineState = nullptr;
    }
    if (m_rootSignature) {
        m_rootSignature->Release();
        m_rootSignature = nullptr;
    }
}

bool DeferredLightingPipeline::LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode) {
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

bool DeferredLightingPipeline::CreateRootSignature(Device& device) {
    D3D12_DESCRIPTOR_RANGE1 ranges[4]{};
    for (uint32_t i = 0; i < 4; ++i) {
        ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[i].NumDescriptors = 1;
        ranges[i].BaseShaderRegister = i; // t0..t3
        ranges[i].RegisterSpace = 0;
        ranges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    D3D12_ROOT_PARAMETER1 params[5]{};
    for (uint32_t i = 0; i < 4; ++i) {
        params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params[i].DescriptorTable.NumDescriptorRanges = 1;
        params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
    }

    params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[4].Descriptor.ShaderRegister = 1; // b1
    params[4].Descriptor.RegisterSpace = 0;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0; // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootDesc.Desc_1_1.NumParameters = _countof(params);
    rootDesc.Desc_1_1.pParameters = params;
    rootDesc.Desc_1_1.NumStaticSamplers = 1;
    rootDesc.Desc_1_1.pStaticSamplers = &sampler;
    rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (!CheckHRESULT(
            D3D12SerializeVersionedRootSignature(&rootDesc, &signature, &error),
            "D3D12SerializeVersionedRootSignature (deferred lighting)")) {
        if (error) {
            spdlog::error("Root signature error: {}", static_cast<const char*>(error->GetBufferPointer()));
        }
        return false;
    }

    return CheckHRESULT(
        device.GetD3D12Device()->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)),
        "CreateRootSignature (deferred lighting)");
}

bool DeferredLightingPipeline::CreatePipelineState(
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
    psoDesc.InputLayout = {nullptr, 0};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    psoDesc.SampleDesc.Count = 1;

    return CheckHRESULT(
        device.GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)),
        "CreateGraphicsPipelineState (deferred lighting)");
}

bool DeferredLightingPipeline::Initialize(
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

    spdlog::info("DeferredLightingPipeline initialized");
    return true;
}

void DeferredLightingPipeline::Draw(
    ID3D12GraphicsCommandList* commandList,
    D3D12_GPU_VIRTUAL_ADDRESS frameCb,
    D3D12_GPU_DESCRIPTOR_HANDLE albedo,
    D3D12_GPU_DESCRIPTOR_HANDLE normal,
    D3D12_GPU_DESCRIPTOR_HANDLE material,
    D3D12_GPU_DESCRIPTOR_HANDLE depth) {
    commandList->SetPipelineState(m_pipelineState);
    commandList->SetGraphicsRootSignature(m_rootSignature);
    commandList->SetGraphicsRootDescriptorTable(0, albedo);
    commandList->SetGraphicsRootDescriptorTable(1, normal);
    commandList->SetGraphicsRootDescriptorTable(2, material);
    commandList->SetGraphicsRootDescriptorTable(3, depth);
    commandList->SetGraphicsRootConstantBufferView(4, frameCb);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 0, nullptr);
    commandList->IASetIndexBuffer(nullptr);
    commandList->DrawInstanced(3, 1, 0, 0);
}

} // namespace summer::gfx
