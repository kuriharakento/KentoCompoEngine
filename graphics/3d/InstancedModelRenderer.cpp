#include "InstancedModelRenderer.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "base/Logger.h"
#include "base/Camera.h"
#include "manager/scene/LightManager.h"
#include "manager/graphics/TextureManager.h"
#include "math/MathUtils.h"
#include "math/MatrixFunc.h"
#include "manager/graphics/InstancedModelPipelineManager.h"
#include "engine/manager/graphics/ShadowMapManager.h"
#include <d3dcompiler.h>
#include <cassert>

namespace KCE
{
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

    // モデルのルートノード行列を取得（座標系変換や初期スケールが含まれる場合がある）
    Matrix4x4 rootMatrix = MakeIdentity4x4();
    if (model_)
    {
        rootMatrix = model_->GetModelData().rootNode.localMatrix;
    }

    for (uint32_t i = 0; i < currentInstanceCount_; ++i)
    {
        // 従来の Object3d と同様、モデル自身のルート行列を適用する
        Matrix4x4 world = Multiply(rootMatrix, matrices[i]);
        Matrix4x4 wvp = Multiply(world, viewProjection);
        
        TransformationMatrix data;
        data.WVP = wvp;
        data.World = world;
        // 法線変換用行列の計算（逆転置）
        data.WorldInverseTranspose = MathUtils::Transpose(Inverse(world));

        mappedData[i] = data;
    }
}

void InstancedModelRenderer::DrawInstanced(Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
    if (currentInstanceCount_ == 0 || !model_)
    {
        return;
    }

    auto* commandList = dxCommon_->GetCommandList();
    auto* pipelineManager = InstancedModelPipelineManager::GetInstance();

    // 1. パイプライン設定
    commandList->SetGraphicsRootSignature(pipelineManager->GetRootSignature());
    commandList->SetPipelineState(pipelineManager->GetPipelineState());
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

void InstancedModelRenderer::DrawInstancedGBuffer(Camera* camera)
{
    if (currentInstanceCount_ == 0 || !model_)
    {
        return;
    }

    auto* commandList = dxCommon_->GetCommandList();
    auto* pipelineManager = InstancedModelPipelineManager::GetInstance();

    // 1. パイプライン設定
    commandList->SetGraphicsRootSignature(pipelineManager->GetRootSignature());
    commandList->SetPipelineState(pipelineManager->GetPipelineStateGBuffer());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 2. 共通定数バインド
    commandList->SetGraphicsRootShaderResourceView(1, instancedResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(4, camera->GetConstantBufferAddress());

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

void InstancedModelRenderer::DrawInstancedShadow(Camera* camera, ShadowMapManager* shadowMapManager)
{
    if (currentInstanceCount_ == 0 || !model_)
    {
        return;
    }

    auto* commandList = dxCommon_->GetCommandList();
    auto* pipelineManager = InstancedModelPipelineManager::GetInstance();

    // 1. パイプライン設定
    commandList->SetGraphicsRootSignature(pipelineManager->GetRootSignature());
    commandList->SetPipelineState(pipelineManager->GetPipelineStateShadow());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 2. 共通定数バインド
    D3D12_GPU_VIRTUAL_ADDRESS shadowMatrixAddr = shadowMapManager ? shadowMapManager->GetCurrentShadowMatrixAddress() : 0;
    if (shadowMatrixAddr != 0)
    {
        commandList->SetGraphicsRootConstantBufferView(0, shadowMatrixAddr);
    }
    commandList->SetGraphicsRootShaderResourceView(1, instancedResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(4, camera->GetConstantBufferAddress());

    // 3. メッシュごとの描画 (影用なのでマテリアルやテクスチャは不要)
    for (const auto& mesh : model_->GetMeshResources())
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = mesh.vertexBufferView;
        commandList->IASetVertexBuffers(0, 1, &vbv);
        commandList->IASetIndexBuffer(&mesh.indexBufferView);

        commandList->DrawIndexedInstanced(mesh.indexCount, currentInstanceCount_, 0, 0, 0);
    }
}
} // namespace KCE
