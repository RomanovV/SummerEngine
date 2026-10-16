#include "Gfx/GeometryPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/GBuffer.h"
#include "Gfx/Helpers.h"
#include "Gfx/Material.h"
#include "Gfx/Texture2D.h"

#include <fstream>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

namespace {

constexpr D3D12_INPUT_ELEMENT_DESC kInputLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

// Mirrors the b2 layout in the shader (12 DWORDs).
struct MaterialConstants {
    float baseColorFactor[4];
    float emissiveFactor[3];
    float metallic;
    float roughness;
    float normalScale;
    float occlusionStrength;
    float _pad;
};

constexpr uint32_t kMaterialTextureCount = 5; // baseColor, normal, metalRough, occlusion, emissive

} // namespace

GeometryPipeline::~GeometryPipeline() {
    Shutdown();
}

void GeometryPipeline::Shutdown() {
    if (m_pipelineState) {
        m_pipelineState->Release();
        m_pipelineState = nullptr;
    }
    if (m_rootSignature) {
        m_rootSignature->Release();
        m_rootSignature = nullptr;
    }
}

bool GeometryPipeline::LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode) {
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

bool GeometryPipeline::CreateRootSignature(Device& device) {
    // One SRV descriptor range per material texture, each its own single-descriptor table.
    D3D12_DESCRIPTOR_RANGE1 srvRanges[kMaterialTextureCount]{};
    for (uint32_t i = 0; i < kMaterialTextureCount; ++i) {
        srvRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRanges[i].NumDescriptors = 1;
        srvRanges[i].BaseShaderRegister = i; // t0..t4
        srvRanges[i].RegisterSpace = 0;
        srvRanges[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        srvRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }

    // Params: [0]=object constants b0, [1]=frame CBV b1, [2]=material constants b2, [3..7]=SRV tables.
    D3D12_ROOT_PARAMETER1 rootParameters[3 + kMaterialTextureCount]{};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Constants.RegisterSpace = 0;
    rootParameters[0].Constants.ShaderRegister = 0; // b0
    rootParameters[0].Constants.Num32BitValues = 32; // model + normalMatrix

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].Descriptor.RegisterSpace = 0;
    rootParameters[1].Descriptor.ShaderRegister = 1; // b1

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].Constants.RegisterSpace = 0;
    rootParameters[2].Constants.ShaderRegister = 2; // b2
    rootParameters[2].Constants.Num32BitValues = sizeof(MaterialConstants) / 4;

    for (uint32_t i = 0; i < kMaterialTextureCount; ++i) {
        D3D12_ROOT_PARAMETER1& param = rootParameters[3 + i];
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        param.DescriptorTable.NumDescriptorRanges = 1;
        param.DescriptorTable.pDescriptorRanges = &srvRanges[i];
    }

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_ANISOTROPIC;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MaxAnisotropy = 8;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0; // s0
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootDesc.Desc_1_1.NumParameters = _countof(rootParameters);
    rootDesc.Desc_1_1.pParameters = rootParameters;
    rootDesc.Desc_1_1.NumStaticSamplers = 1;
    rootDesc.Desc_1_1.pStaticSamplers = &sampler;
    rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (!CheckHRESULT(
            D3D12SerializeVersionedRootSignature(&rootDesc, &signature, &error),
            "D3D12SerializeVersionedRootSignature (geometry)")) {
        if (error) {
            spdlog::error("Root signature error: {}", static_cast<const char*>(error->GetBufferPointer()));
        }
        return false;
    }

    return CheckHRESULT(
        device.GetD3D12Device()->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)),
        "CreateRootSignature (geometry)");
}

bool GeometryPipeline::CreatePipelineState(
    Device& device,
    const std::vector<uint8_t>& vsBytecode,
    const std::vector<uint8_t>& psBytecode) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = m_rootSignature;
    psoDesc.VS = {vsBytecode.data(), vsBytecode.size()};
    psoDesc.PS = {psBytecode.data(), psBytecode.size()};
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = {kInputLayout, _countof(kInputLayout)};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 3;
    psoDesc.RTVFormats[0] = GBuffer::kAlbedoFormat;
    psoDesc.RTVFormats[1] = GBuffer::kNormalFormat;
    psoDesc.RTVFormats[2] = GBuffer::kMaterialFormat;
    psoDesc.DSVFormat = GBuffer::kDepthFormat;
    psoDesc.SampleDesc.Count = 1;

    return CheckHRESULT(
        device.GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)),
        "CreateGraphicsPipelineState (geometry)");
}

bool GeometryPipeline::Initialize(
    Device& device,
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
    if (!CreatePipelineState(device, vsBytecode, psBytecode)) {
        Shutdown();
        return false;
    }

    spdlog::info("GeometryPipeline initialized");
    return true;
}

void GeometryPipeline::Bind(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(m_pipelineState);
    commandList->SetGraphicsRootSignature(m_rootSignature);
}

void GeometryPipeline::SetObjectConstants(
    ID3D12GraphicsCommandList* commandList,
    const DirectX::XMFLOAT4X4& model,
    const DirectX::XMFLOAT4X4& normalMatrix) {
    commandList->SetGraphicsRoot32BitConstants(0, 16, &model, 0);
    commandList->SetGraphicsRoot32BitConstants(0, 16, &normalMatrix, 16);
}

void GeometryPipeline::SetFrameConstants(
    ID3D12GraphicsCommandList* commandList,
    D3D12_GPU_VIRTUAL_ADDRESS frameCbAddress) {
    commandList->SetGraphicsRootConstantBufferView(1, frameCbAddress);
}

void GeometryPipeline::SetMaterial(ID3D12GraphicsCommandList* commandList, const Material& material) {
    MaterialConstants constants{};
    constants.baseColorFactor[0] = material.baseColorFactor.x;
    constants.baseColorFactor[1] = material.baseColorFactor.y;
    constants.baseColorFactor[2] = material.baseColorFactor.z;
    constants.baseColorFactor[3] = material.baseColorFactor.w;
    constants.emissiveFactor[0] = material.emissiveFactor.x;
    constants.emissiveFactor[1] = material.emissiveFactor.y;
    constants.emissiveFactor[2] = material.emissiveFactor.z;
    constants.metallic = material.metallicFactor;
    constants.roughness = material.roughnessFactor;
    constants.normalScale = material.normalScale;
    constants.occlusionStrength = material.occlusionStrength;
    commandList->SetGraphicsRoot32BitConstants(2, sizeof(MaterialConstants) / 4, &constants, 0);

    const Texture2D* textures[kMaterialTextureCount] = {
        material.baseColor, material.normal, material.metalRough,
        material.occlusion, material.emissive};
    for (uint32_t i = 0; i < kMaterialTextureCount; ++i) {
        if (textures[i]) {
            commandList->SetGraphicsRootDescriptorTable(3 + i, textures[i]->GetSrvGpu());
        }
    }
}

} // namespace summer::gfx