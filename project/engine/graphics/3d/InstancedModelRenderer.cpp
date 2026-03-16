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
    : m_maxInstances(maxInstances)
    , m_currentInstanceCount(0)
{
}

InstancedModelRenderer::~InstancedModelRenderer()
{
}

void InstancedModelRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Model* model)
{
    m_dxCommon = dxCommon;
    m_srvManager = srvManager;
    m_model = model;

    // 1. Structured Buffer の作成 (TransformationMatrix 構造体を使用)
    // GraphicsTypes.h の TransformationMatrix と合わせる
    size_t elementSize = sizeof(TransformationMatrix);
    size_t bufferSize = elementSize * m_maxInstances;
    m_instancedResource = m_dxCommon->CreateBufferResource(bufferSize);

    // 2. Map してポインタを保持
    m_instancedResource->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedMatrices));

    // 3. SRV の作成
    m_srvIndex = m_srvManager->Allocate();
    m_srvManager->CreateSRVforStructuredBuffer(m_srvIndex, m_instancedResource.Get(), m_maxInstances, (UINT)elementSize);

    // 4. Root Signature と PSO の作成
    CreateRootSignature();
    CreatePipelineState();
}

void InstancedModelRenderer::UpdateBuffer(const Matrix4x4* matrices, uint32_t count, Camera* camera)
{
    m_currentInstanceCount = (count > m_maxInstances) ? m_maxInstances : count;
    if (m_currentInstanceCount == 0 || !m_mappedMatrices || !camera) return;

    Matrix4x4 viewProjection = camera->GetViewProjectionMatrix();

    TransformationMatrix* mappedData = reinterpret_cast<TransformationMatrix*>(m_mappedMatrices);

    for (uint32_t i = 0; i < m_currentInstanceCount; ++i)
    {
        Matrix4x4 world = matrices[i];
        Matrix4x4 wvp = Multiply(world, viewProjection);
        
        // 構造体の各メンバに書き込み
        // TransformationMatrix は WVP, World, WorldInverseTranspose の順
        TransformationMatrix data;
        data.WVP = wvp;
        data.World = world;
        data.WorldInverseTranspose = MathUtils::Transpose(Inverse(world));

        // 構造体ごと一括で書き込む
        mappedData[i] = data;
    }
}

#include "manager/graphics/ShadowMapManager.h"
#include "manager/graphics/TextureManager.h"

void InstancedModelRenderer::DrawInstanced(Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
    if (m_currentInstanceCount == 0 || !m_model) return;

    auto* commandList = m_dxCommon->GetCommandList();

    // 1. パイプライン設定
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 2. 共通定数バインド
    // RootParam 1: InstanceMatrices (SRV t0) - VS
    commandList->SetGraphicsRootShaderResourceView(1, m_instancedResource->GetGPUVirtualAddress());

    // RootParam 4: Camera (CBV b2) - PS
    commandList->SetGraphicsRootConstantBufferView(4, camera->GetConstantBufferAddress());

    // 環境マップ (RootParam 8)
    commandList->SetGraphicsRootDescriptorTable(8, m_srvManager->GetGPUDescriptorHandle(TextureManager::GetInstance()->GetSRVIndex("./Resources/rostock_laage_airport_4k.dds")));

    // ライトのバインド
    if (lightManager)
    {
        // RootParam 3: DirectionalLight (CBV b1)
        commandList->SetGraphicsRootConstantBufferView(3, lightManager->GetDirectionalLightGPUAddress());
        // RootParam 7: LightCount (CBV b5)
        commandList->SetGraphicsRootConstantBufferView(7, lightManager->GetLightCountResource()->GetGPUVirtualAddress());
        
        // RootParam 5: PointLights (SRV t3)
        commandList->SetGraphicsRootShaderResourceView(5, lightManager->GetPointLightResource()->GetGPUVirtualAddress());
        // RootParam 6: SpotLights (SRV t4)
        commandList->SetGraphicsRootShaderResourceView(6, lightManager->GetSpotLightResource()->GetGPUVirtualAddress());

        // RootParam 10: ShadowMatrix (CBV b6)
        commandList->SetGraphicsRootConstantBufferView(10, lightManager->GetShadowMatrixGPUAddress());
        // RootParam 11: CascadeShadowData (CBV b7)
        commandList->SetGraphicsRootConstantBufferView(11, lightManager->GetCascadeShadowDataGPUAddress());
    }

    // シャドウマップのバインド (RootParam 9, 12-15)
    if (shadowMapManager)
    {
        if (shadowMapManager->HasDirectionalLightShadowMap())
        {
            commandList->SetGraphicsRootDescriptorTable(9, m_srvManager->GetGPUDescriptorHandle(shadowMapManager->GetDirectionalLightShadowMap().srvIndex));
        }

        if (shadowMapManager->HasCascadeShadowMaps())
        {
            const auto& cascade = shadowMapManager->GetCascadeShadowMap();
            for (int i = 0; i < 4; ++i)
            {
                commandList->SetGraphicsRootDescriptorTable(12 + i, m_srvManager->GetGPUDescriptorHandle(cascade.srvIndices[i]));
            }
        }
    }

    // 3. メッシュごとの描画
    for (const auto& mesh : m_model->GetMeshResources())
    {
        // RootParam 0: Material (CBV b0)
        commandList->SetGraphicsRootConstantBufferView(0, mesh.materialBuffer->GetGPUVirtualAddress());

        // RootParam 2: Texture (DescriptorTable t0)
        commandList->SetGraphicsRootDescriptorTable(2, m_srvManager->GetGPUDescriptorHandle(mesh.textureIndex));

        // VB/IB 設定
        D3D12_VERTEX_BUFFER_VIEW vbv = mesh.vertexBufferView;
        commandList->IASetVertexBuffers(0, 1, &vbv);
        commandList->IASetIndexBuffer(&mesh.indexBufferView);

        // Draw Call
        commandList->DrawIndexedInstanced(mesh.indexCount, m_currentInstanceCount, 0, 0, 0);
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
    descriptorRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

    CD3DX12_DESCRIPTOR_RANGE descriptorRangeEnvMap[1] = {};
    descriptorRangeEnvMap[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

    CD3DX12_DESCRIPTOR_RANGE descriptorRangeShadowMap[1] = {};
    descriptorRangeShadowMap[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5); // t5

    CD3DX12_DESCRIPTOR_RANGE descriptorRangeCascade[4] = {};
    for (int i = 0; i < 4; ++i) {
        descriptorRangeCascade[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6 + i); // t6-t9
    }

    CD3DX12_ROOT_PARAMETER rootParameters[16] = {};

    // 0: Material (CBV b0)
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 1: InstanceMatrices (SRV t0) - VS (Root SRV)
    rootParameters[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    // 2: Texture (DescriptorTable t0) - PS
    rootParameters[2].InitAsDescriptorTable(1, &descriptorRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
    // 3: DirectionalLight (CBV b1)
    rootParameters[3].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 4: Camera (CBV b2)
    rootParameters[4].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 5: PointLights (SRV t3)
    rootParameters[5].InitAsShaderResourceView(3, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 6: SpotLights (SRV t4)
    rootParameters[6].InitAsShaderResourceView(4, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 7: LightCount (CBV b5)
    rootParameters[7].InitAsConstantBufferView(5, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 8: EnvMap (t1)
    rootParameters[8].InitAsDescriptorTable(1, &descriptorRangeEnvMap[0], D3D12_SHADER_VISIBILITY_PIXEL);
    // 9: ShadowMap (t5)
    rootParameters[9].InitAsDescriptorTable(1, &descriptorRangeShadowMap[0], D3D12_SHADER_VISIBILITY_PIXEL);
    // 10: ShadowMatrix (CBV b6)
    rootParameters[10].InitAsConstantBufferView(6, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 11: CascadeShadowData (CBV b7)
    rootParameters[11].InitAsConstantBufferView(7, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    // 12-15: CascadeShadowMaps (t6-t9)
    for (int i = 0; i < 4; ++i) {
        rootParameters[12 + i].InitAsDescriptorTable(1, &descriptorRangeCascade[i], D3D12_SHADER_VISIBILITY_PIXEL);
    }

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.Init(_countof(rootParameters), rootParameters, _countof(staticSamplers), staticSamplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) Logger::Log(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }
    m_dxCommon->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
}

void InstancedModelRenderer::CreatePipelineState()
{
    auto vsBlob = m_dxCommon->CompileSharder(L"Resources/shaders/InstancedObject3d.VS.hlsl", L"vs_6_0");
    auto psBlob = m_dxCommon->CompileSharder(L"Resources/shaders/InstancedObject3d.PS.hlsl", L"ps_6_0");
    assert(vsBlob && "Failed to compile VS");
    assert(psBlob && "Failed to compile PS");

    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
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

    HRESULT hr = m_dxCommon->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    assert(SUCCEEDED(hr));
}