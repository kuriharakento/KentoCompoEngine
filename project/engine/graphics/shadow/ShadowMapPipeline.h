#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "base/DirectXCommon.h"

/**
 * @brief シャドウマップ用パイプラインクラス
 * @details シャドウマップ描画に必要なルートシグネチャとパイプラインステートを管理
 */
class ShadowMapPipeline
{
public:
	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief パイプラインの設定
	 * @details コマンドリストにルートシグネチャとパイプラインステートを設定
	 */
	void SetPipeline() const;

	/**
	 * @brief ライトビュープロジェクション行列の定数バッファを設定
	 * @param gpuVirtualAddress 定数バッファのGPUアドレス
	 */
	void SetLightViewProjection(D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress) const;

	/**
	 * @brief ワールド行列の定数バッファを設定
	 * @param gpuVirtualAddress 定数バッファのGPUアドレス
	 */
	void SetWorldMatrix(D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress) const;

public: // ルートパラメータインデックス
	static constexpr uint32_t kRootParamLightViewProjection = 0;
	static constexpr uint32_t kRootParamWorldMatrix = 1;

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
	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// パイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
