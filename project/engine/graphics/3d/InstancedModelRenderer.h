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
     * @brief 初期化処理。Structured Buffer の生成などを行う。
     * @param dxCommon DirectXCommonへのポインタ
     * @param srvManager SRVマネージャーへのポインタ
     * @param model 使用するモデル
     */
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Model* model);

    /**
     * @brief 描画する対象の行列一覧をGPUバッファに転送する。
     * @param matrices 描画対象のTransform(ワールド行列)配列
     * @param count 有効なインスタンス数
     * @param camera 使用するカメラ（WVP計算用）
     */
    void UpdateBuffer(const Matrix4x4* matrices, uint32_t count, Camera* camera);

    /**
     * @brief グラフィックスコマンドリストに一括描画命令（DrawInstanced）を積む。
     * @param camera 使用するカメラ
     * @param lightManager 使用するライトマネージャー
     * @param shadowMapManager シャドウマップマネージャー
     */
    void DrawInstanced(Camera* camera, LightManager* lightManager, class ShadowMapManager* shadowMapManager);

private:
    /**
     * @brief ルートシグネチャの作成
     */
    void CreateRootSignature();

    /**
     * @brief パイプラインステートの作成
     */
    void CreatePipelineState();

private:
    // エンジン共通コンポーネント
    DirectXCommon* m_dxCommon = nullptr;
    SrvManager* m_srvManager = nullptr;
    Model* m_model = nullptr;

    const uint32_t m_maxInstances;
    uint32_t m_currentInstanceCount = 0;

    // Direct3D12 リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> m_instancedResource;
    Matrix4x4* m_mappedMatrices = nullptr;
    uint32_t m_srvIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
};