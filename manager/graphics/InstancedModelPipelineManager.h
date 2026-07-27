#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "base/DirectXCommon.h"

namespace KCE
{
/**
 * @brief インスタンス描画用パイプラインマネージャー
 * @details インスタンス描画に必要なルートシグネチャとPSOを管理するシングルトンクラス
 */
class InstancedModelPipelineManager
{
public:
	/**
	 * @brief インスタンスを取得
	 * @return マネージャーのインスタンス
	 */
	static InstancedModelPipelineManager* GetInstance();

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
	ID3D12PipelineState* GetPipelineStateGBuffer() const { return pipelineStateGBuffer_.Get(); }
	ID3D12PipelineState* GetPipelineStateShadow() const { return pipelineStateShadow_.Get(); }

private:
	InstancedModelPipelineManager() = default;
	~InstancedModelPipelineManager() = default;
	InstancedModelPipelineManager(const InstancedModelPipelineManager&) = delete;
	InstancedModelPipelineManager& operator=(const InstancedModelPipelineManager&) = delete;

	/**
	 * @brief パイプラインを生成
	 */
	void CreatePipeline();

private:
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateGBuffer_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateShadow_;
};
} // namespace KCE
