#include "InstancedModelRenderer.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "base/Logger.h"
#include "base/Camera.h"
#include "manager/scene/LightManager.h"
#include "manager/graphics/TextureManager.h"
#include "math/MathUtils.h"
#include "math/MatrixFunc.h"
#include "externals/DirectXTex/d3dx12.h"
#include <d3dcompiler.h>
#include <cassert>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

InstancedModelRenderer::InstancedModelRenderer(uint32_t maxInstances)
    : maxInstances_(maxInstances)
    , currentInstanceCount_(0)
{
}

InstancedModelRenderer::~InstancedModelRenderer()
{
}

void InstancedModelRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Model* model)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    model_ = model;

    // Structured Buffer の作成
    size_t elementSize = sizeof(TransformationMatrix);
    size_t bufferSize = elementSize * maxInstances_;
    instancedResource_ = dxCommon_->CreateBufferResource(bufferSize);

    // Map
    instancedResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedMatrices_));

    // SRV の作成
    srvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(srvIndex_, instancedResource_.Get(), maxInstances_, (UINT)elementSize);

    CreateRootSignature();
    CreatePipelineState();
}

void InstancedModelRenderer::UpdateBuffer(const Matrix4x4* matrices, uint32_t count, Camera* camera)
{
    currentInstanceCount_ = (count > maxInstances_) ? maxInstances_ : count;
    if (currentInstanceCount_ == 0 || !mappedMatrices_ || !camera)
    {
        return;
    }

    Matrix4x4 viewProjection = camera->GetViewProjectionMatrix();
    TransformationMatrix* mappedData = reinterpret_cast<TransformationMatrix*>(mappedMatrices_);

    for (uint32_t i = 0; i < currentInstanceCount_; ++i)
    {
        Matrix4x4 world = matrices[i];
        Matrix4x4 wvp = Multiply(world, viewProjection);
        
        TransformationMatrix data;
        data.WVP = wvp;
        data.World = world;
        data.WorldInverseTranspose = MathUtils::Transpose(Inverse(world));

        mappedData[i] = data;
    }
}

#include "manager/graphics/ShadowMapManager.h"
#include "manager/graphics/TextureManager.h"

void InstancedModelRenderer::DrawInstanced(Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
    if (currentInstanceCount_ == 0 || !model_)
    {
        return;
    }

    auto* commandList = dxCommon_->GetCommandList();

    // 1. パイプライン設定
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 2. 共通定数バインド
    commandList->SetGraphicsRootShaderResourceView(1, instancedResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(4, camera->GetConstantBufferAddress());

    // 環境マップ
    commandList->SetGraphicsRootDescriptorTable(8, srvManager_->GetGPUDescriptorHandle(TextureManager::GetInstance()->GetSRVIndex("./Resources/rostock_laage_airport_4k.dds")));

    // ライトのバインド
    if (lightManager)
    {
        commandList->SetGraphicsRootConstantBufferView(3, lightManager->GetDirectionalLightGPUAddress());
        commandList->SetGraphicsRootConstantBufferView(7, lightManager->GetLightCountResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootShaderResourceView(5, lightManager->GetPointLightResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootShaderResourceView(6, lightManager->GetSpotLightResource()->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(10, lightManager->GetShadowMatrixGPUAddress());
        commandList->SetGraphicsRootConstantBufferView(11, lightManager->GetCascadeShadowDataGPUAddress());
    }

    // シャドウマップのバインド
    if (shadowMapManager)
    {
        if (shadowMapManager->HasDirectionalLightShadowMap())
        {
            commandList->SetGraphicsRootDescriptorTable(9, srvManager_->GetGPUDescriptorHandle(shadowMapManager->GetDirectionalLightShadowMap().srvIndex));
        }

        if (shadowMapManager->HasCascadeShadowMaps())
        {
            const auto& cascade = shadowMapManager->GetCascadeShadowMap();
            for (int i = 0; i < 4; ++i)
            {
                commandList->SetGraphicsRootDescriptorTable(12 + i, srvManager_->GetGPUDescriptorHandle(cascade.srvIndices[i]));
            }
        }
    }

    // 3. メッシュごとの描画
    for (const auto& mesh : model_->GetMeshResources())
    {
        commandList->SetGraphicsRootConstantBufferView(0, mesh.materialBuffer->GetGPUVirtualAddress());
        commandList->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(mesh.textureIndex));

        D3D12_VERTEX_BUFFER_VIEW vbv = mesh.vertexBufferView;
        commandList->IASetVertexBuffers(0, 1, &vbv);
        commandList->IASetIndexBuffer(&mesh.indexBufferView);

        commandList->DrawIndexedInstanced(mesh.indexCount, currentInstanceCount_, 0, 0, 0);
    }
}

void InstancedModelRenderer::CreateRootSignature()
{
    // 通常サンプラー + シャドウ比較サンプラー
    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    staticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[1].ShaderRegister = 1;
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ディスクリプタレンジ
    CD3DX12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE descriptorRangeEnvMap[1] = {};
    descriptorRangeEnvMap[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_DESCRIPTOR_RANGE descriptorRangeShadowMap[1] = {};
    descriptorRangeShadowMap[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);

    CD3DX12_DESCRIPTOR_RANGE descriptorRangeCascade[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        descriptorRangeCascade[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6 + i);
    }

    CD3DX12_ROOT_PARAMETER rootParameters[16] = {};

    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[2].InitAsDescriptorTable(1, &descriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[3].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[4].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[5].InitAsShaderResourceView(3, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[6].InitAsShaderResourceView(4, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[7].InitAsConstantBufferView(5, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[8].InitAsDescriptorTable(1, &descriptorRangeEnvMap[0], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[9].InitAsDescriptorTable(1, &descriptorRangeShadowMap[0], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[10].InitAsConstantBufferView(6, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[11].InitAsConstantBufferView(7, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    
    for (int i = 0; i < 4; ++i)
    {
        rootParameters[12 + i].InitAsDescriptorTable(1, &descriptorRangeCascade[i], D3D12_SHADER_VISIBILITY_PIXEL);
    }

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.Init(_countof(rootParameters), rootParameters, _countof(staticSamplers), staticSamplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            Logger::Log(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
}

void InstancedModelRenderer::CreatePipelineState()
{
    auto vsBlob = dxCommon_->CompileSharder(L"Resources/shaders/InstancedObject3d.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileSharder(L"Resources/shaders/InstancedObject3d.PS.hlsl", L"ps_6_0");
    assert(vsBlob && "Failed to compile VS");
    assert(psBlob && "Failed to compile PS");

    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    
    // アルファブレンド設定
    D3D12_RENDER_TARGET_BLEND_DESC& blend = psoDesc.BlendState.RenderTarget[0];
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}