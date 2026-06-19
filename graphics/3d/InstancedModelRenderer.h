#pragma once

#include <vector>
#include <DirectXMath.h>
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include "graphics/3d/Model.h"

class Camera;
class LightManager;
class SrvManager;
class DirectXCommon;

/**
 * @brief 同じモデル群のTransform行列をStructured Bufferにまとめて一括描画するレンダラ基盤。
 */
class InstancedModelRenderer
{
public:
    InstancedModelRenderer(uint32_t maxInstances);
    ~InstancedModelRenderer();

    /**
     * @brief 初期化
     * @param dxCommon DirectXCommonへのポインタ
     * @param srvManager SRVマネージャーへのポインタ
     * @param model 使用するモデル
     */
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Model* model);

    /**
     * @brief 行列バッファの更新
     * @param matrices 描画対象のTransform(ワールド行列)配列
     * @param count 有効なインスタンス数
     * @param camera 使用するカメラ（WVP計算用）
     */
    void UpdateBuffer(const Matrix4x4* matrices, uint32_t count, Camera* camera);

    /**
     * @brief インスタンス描画命令の発行
     * @param camera 使用するカメラ
     * @param lightManager 使用するライトマネージャー
     * @param shadowMapManager シャドウマップマネージャー
     */
    void DrawInstanced(Camera* camera, LightManager* lightManager, class ShadowMapManager* shadowMapManager);
    void DrawInstancedGBuffer(Camera* camera);
    void DrawInstancedShadow(Camera* camera, class ShadowMapManager* shadowMapManager);

private:

private:
    // エンジン共通コンポーネント
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Model* model_ = nullptr;

    const uint32_t maxInstances_;
    uint32_t currentInstanceCount_ = 0;

    // Direct3D12 リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancedResource_;
    Matrix4x4* mappedMatrices_ = nullptr;
    uint32_t srvIndex_ = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};