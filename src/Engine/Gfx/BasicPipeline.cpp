#include "Gfx/BasicPipeline.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <d3d12shader.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

namespace {

constexpr D3D12_INPUT_ELEMENT_DESC kInputLayout[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

} // namespace

BasicPipeline::~BasicPipeline() {
    Shutdown();
}

void BasicPipeline::Shutdown() {
    if (m_constantBufferMapped && m_constantBuffer) {
        m_constantBuffer->Unmap(0, nullptr);
        m_constantBufferMapped = nullptr;
    }

    if (m_constantBuffer) {
        m_constantBuffer->Release();
        m_constantBuffer = nullptr;
    }

    if (m_pipelineState) {
        m_pipelineState->Release();
        m_pipelineState = nullptr;
    }

    if (m_rootSignature) {
        m_rootSignature->Release();
        m_rootSignature = nullptr;
    }
}

bool BasicPipeline::LoadShaderBytecode(const char* path, std::vector<uint8_t>& outBytecode) {
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

bool BasicPipeline::CreateRootSignature(Device& device) {
    D3D12_ROOT_PARAMETER1 rootParameter{};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameter.Descriptor.RegisterSpace = 0;
    rootParameter.Descriptor.ShaderRegister = 0;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootDesc.Desc_1_1.NumParameters = 1;
    rootDesc.Desc_1_1.pParameters = &rootParameter;
    rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (!CheckHRESULT(
            D3D12SerializeVersionedRootSignature(&rootDesc, &signature, &error),
            "D3D12SerializeVersionedRootSignature")) {
        if (error) {
            spdlog::error("Root signature error: {}", static_cast<const char*>(error->GetBufferPointer()));
        }
        return false;
    }

    if (!CheckHRESULT(
            device.GetD3D12Device()->CreateRootSignature(
                0,
                signature->GetBufferPointer(),
                signature->GetBufferSize(),
                IID_PPV_ARGS(&m_rootSignature)),
            "CreateRootSignature")) {
        return false;
    }

    D3D12_HEAP_PROPERTIES uploadHeapProps{};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC constantBufferDesc{};
    constantBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    constantBufferDesc.Width = sizeof(FrameConstants);
    constantBufferDesc.Height = 1;
    constantBufferDesc.DepthOrArraySize = 1;
    constantBufferDesc.MipLevels = 1;
    constantBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    constantBufferDesc.SampleDesc.Count = 1;
    constantBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (!CheckHRESULT(
            device.GetD3D12Device()->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &constantBufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_constantBuffer)),
            "CreateCommittedResource constant buffer")) {
        return false;
    }

    D3D12_RANGE readRange{0, 0};
    if (!CheckHRESULT(
            m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_constantBufferMapped)),
            "Map constant buffer")) {
        return false;
    }

    return true;
}

bool BasicPipeline::CreatePipelineState(
    Device& device,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
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
    // Our OBJ meshes author front faces as counter-clockwise; with the LH view/proj that
    // makes CCW the front-facing winding. FALSE here would cull the visible faces instead.
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.RasterizerState.DepthBias = 0;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = {kInputLayout, _countof(kInputLayout)};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc.Count = 1;

    if (!CheckHRESULT(
            device.GetD3D12Device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)),
            "CreateGraphicsPipelineState")) {
        return false;
    }

    return true;
}

bool BasicPipeline::Initialize(
    Device& device,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat,
    const char* vertexShaderPath,
    const char* pixelShaderPath) {
    Shutdown();

    std::vector<uint8_t> vsBytecode;
    std::vector<uint8_t> psBytecode;
    if (!LoadShaderBytecode(vertexShaderPath, vsBytecode)) {
        return false;
    }
    if (!LoadShaderBytecode(pixelShaderPath, psBytecode)) {
        return false;
    }

    if (!CreateRootSignature(device)) {
        return false;
    }

    if (!CreatePipelineState(device, rtvFormat, dsvFormat, vsBytecode, psBytecode)) {
        Shutdown();
        return false;
    }

    spdlog::info("BasicPipeline initialized");
    return true;
}

void BasicPipeline::Bind(ID3D12GraphicsCommandList* commandList) {
    commandList->SetPipelineState(m_pipelineState);
    commandList->SetGraphicsRootSignature(m_rootSignature);
}

void BasicPipeline::UpdateConstants(
    ID3D12GraphicsCommandList* commandList,
    const DirectX::XMFLOAT4X4& mvp,
    const DirectX::XMFLOAT4& meshColor) {
    auto* constants = reinterpret_cast<FrameConstants*>(m_constantBufferMapped);
    constants->mvp = mvp;
    constants->meshColor = meshColor;

    commandList->SetGraphicsRootConstantBufferView(
        0,
        m_constantBuffer->GetGPUVirtualAddress());
}

} // namespace summer::gfx
