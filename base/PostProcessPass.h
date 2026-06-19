#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <string>

class DirectXCommon;
class SrvManager;

/**
 * @brief ポストプロセスパスクラス
 */
class PostProcessPass
{
public:
    /**
     * @brief 初期化
     * @param dxCommon DirectXCommonへのポインタ
     * @param srvManager SrvManagerへのポインタ
     * @param vsPath 頂点シェーダーのファイルパス
     * @param psPath ピクセルシェーダーのファイルパス
     */
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::wstring& vsPath, const std::wstring& psPath);

    /**
     * @brief 描画処理
     * @param srvHandle シェーダーリソースビューのGPUハンドル
     */
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE srvHandle);

    /**
     * @brief グレースケールエフェクトの有効/無効を設定
     * @param use 有効にする場合はtrue
     */
    void SetGrayscale(bool use);

private:
    /**
     * @brief 定数バッファを更新
     */
	void UpdateConstantBuffer();

private:
    /**
     * @brief エフェクト設定構造体
     */
    struct EffectSettings
    {
        // グレースケールエフェクト有効フラグ
        bool useGrayscale = false;
    } effectSettings_;

    // DirectXCommonへのポインタ
    DirectXCommon* dxCommon_ = nullptr;
    // SrvManagerへのポインタ
    SrvManager* srvManager_ = nullptr;
    // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    // パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    // 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

 
};

