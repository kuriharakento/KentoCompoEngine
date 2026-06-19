#pragma once
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;

/**
 * @brief ライトパス用パイプラインクラス
 * @details ディファードライティングのルートシグネチャとPSOを管理
 */
class LightPassPipeline
{
public:
	LightPassPipeline() = default;
	~LightPassPipeline() = default;

	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief パイプラインをセット
	 */
	void SetPipeline();

	/**
	 * @brief ルートシグネチャの取得
	 */
	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

	/**
	 * @brief PSOの取得
	 */
	ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

private:
	void CreateRootSignature();
	void CreatePipelineState();

private:
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
