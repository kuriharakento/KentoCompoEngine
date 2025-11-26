#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>

#include "math/BlendMode.h"

class DirectXCommon;

/**
 * @brief パーティクル用パイプラインマネージャークラス
 * @details ブレンドモード別のパイプラインステートを管理する
 *          Alpha、Additive、Multiply等の各ブレンドモードに対応した
 *          パイプラインステートを生成・保持する
 */
class ParticlePipelineManager
{
public:
    ParticlePipelineManager() = default;  // コンストラクタ
    ~ParticlePipelineManager() = default; // デストラクタ

    /**
     * @brief 初期化処理
     * @param dxCommon DirectXCommonへのポインタ
     * @details ルートシグネチャと各ブレンドモードのパイプラインステートを生成する
     */
    void Initialize(DirectXCommon* dxCommon);

    /**
     * @brief パイプラインステートの取得
     * @param mode 取得したいブレンドモード
     * @return 指定されたブレンドモードのパイプラインステート
     */
	ID3D12PipelineState* GetPipelineState(BlendMode mode) const;

    /**
     * @brief ルートシグネチャの取得
     * @return ルートシグネチャへのポインタ
     */
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

private:
    /**
     * @brief ルートシグネチャの生成
     */
    void CreateRootSignature();

    /**
     * @brief グラフィックスパイプラインステートの生成
     * @param mode 生成するブレンドモード
     */
	void CreateGraphicsPipelineState(BlendMode mode);

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // ルートシグネチャ
    std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelines_; // ブレンドモード別パイプラインステート
	DirectXCommon* dxCommon_ = nullptr; // DirectXCommonへのポインタ
};
