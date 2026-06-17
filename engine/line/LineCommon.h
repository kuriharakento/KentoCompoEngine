#pragma once
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;

/**
 * @brief ライン描画用の共通リソースを管理するクラス
 * 
 * ライン描画に必要なルートシグネチャとパイプラインステートを
 * 作成・保持します。複数のLineインスタンスで共有して使用します。
 */
class LineCommon {
public:
    /**
     * @brief 初期化
     * @param dxCommon DirectXCommonインスタンス
     */
    void Initialize(DirectXCommon* dxCommon);

    /**
     * @brief パイプラインステートを取得
     * @return パイプラインステートオブジェクト
     */
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const { return pipelineState_; }

    /**
     * @brief ルートシグネチャを取得
     * @return ルートシグネチャオブジェクト
     */
    Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const { return rootSignature_; }

    /**
     * @brief DirectXCommonを取得
     * @return DirectXCommonへのポインタ
     */
    DirectXCommon* GetDirectXCommon() const { return dxCommon_; }

private:
    void CreateGraphicsPipelineState(); // パイプラインステートの作成
    void CreateRootSignature();         // ルートシグネチャの作成

    DirectXCommon* dxCommon_ = nullptr;                          // DirectXCommon
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;  // パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;  // ルートシグネチャ
};
