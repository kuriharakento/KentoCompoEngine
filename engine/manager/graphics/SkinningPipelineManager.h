#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "base/DirectXCommon.h"

/**
 * @brief スキニング計算用パイプラインマネージャー
 * @details コンピュートシェーダーによるスキニングに必要なルートシグネチャとPSOを管理するシングルトンクラス
 */
class SkinningPipelineManager
{
public:
	/**
	 * @brief インスタンスを取得
	 * @return マネージャーのインスタンス
	 */
	static SkinningPipelineManager* GetInstance();

	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief ルートシグネチャを取得
	 * @return ID3D12RootSignature* ルートシグネチャ
	 */
	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

	/**
	 * @brief パイプラインステートを取得
	 * @return ID3D12PipelineState* パイプラインステート
	 */
	ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

private:
	SkinningPipelineManager() = default;
	~SkinningPipelineManager() = default;
	SkinningPipelineManager(const SkinningPipelineManager&) = delete;
	SkinningPipelineManager& operator=(const SkinningPipelineManager&) = delete;

	/**
	 * @brief パイプラインを生成
	 */
	void CreatePipeline();

private:
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
