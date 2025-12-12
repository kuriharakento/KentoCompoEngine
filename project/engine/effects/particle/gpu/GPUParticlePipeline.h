#pragma once
/**
 * @file GPUParticlePipeline.h
 * @brief GPUパーティクルパイプラインマネージャー
 * 
 * コンピュートシェーダー、ルートシグネチャ、PSOなどの
 * 共有リソースを管理。シングルトン実装。
 */
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

class DirectXCommon;

/**
 * @brief GPUパーティクルパイプラインマネージャー
 * 
 * シェーダー、ルートシグネチャ、PSOなどの共有リソースを管理。
 * 全エミッターで同じパイプラインを使い回す。
 */
class GPUParticlePipeline
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return GPUParticlePipelineインスタンス
	 */
	static GPUParticlePipeline* GetInstance();

	/**
	 * @brief パイプラインを初期化
	 * @param dxCommon DirectXCommonインスタンス
	 */
	void Initialize(DirectXCommon* dxCommon);
	
	/**
	 * @brief パイプラインを終了・解放
	 */
	void Finalize();

	/**
	 * @brief ルートシグネチャを取得
	 * @return ルートシグネチャポインタ
	 */
	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
	
	/**
	 * @brief パイプラインステートを取得
	 * @return パイプラインステートポインタ
	 */
	ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }
	
	/**
	 * @brief コンバーター用ルートシグネチャを取得
	 * @return ルートシグネチャポインタ
	 */
	ID3D12RootSignature* GetConverterRootSignature() const { return converterRootSignature_.Get(); }
	
	/**
	 * @brief コンバーター用パイプラインステートを取得
	 * @return パイプラインステートポインタ
	 */
	ID3D12PipelineState* GetConverterPipelineState() const { return converterPipelineState_.Get(); }
	
	/**
	 * @brief パイプラインが有効かどうかを判定
	 * @return 有効な場合true
	 */
	bool IsValid() const { return pipelineState_ != nullptr; }

private:
	GPUParticlePipeline() = default;
	~GPUParticlePipeline() = default;
	GPUParticlePipeline(const GPUParticlePipeline&) = delete;
	GPUParticlePipeline& operator=(const GPUParticlePipeline&) = delete;

	/**
	 * @brief シミュレーション用シェーダーをコンパイル
	 */
	void CompileShader();
	
	/**
	 * @brief シミュレーション用ルートシグネチャを作成
	 */
	void CreateRootSignature();
	
	/**
	 * @brief シミュレーション用パイプラインステートを作成
	 */
	void CreatePipelineState();

	/**
	 * @brief コンバーター用シェーダーをコンパイル
	 */
	void CompileConverterShader();
	
	/**
	 * @brief コンバーター用ルートシグネチャを作成
	 */
	void CreateConverterRootSignature();
	
	/**
	 * @brief コンバーター用パイプラインステートを作成
	 */
	void CreateConverterPipelineState();

private:
	static GPUParticlePipeline* instance_;
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> converterRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> converterPipelineState_;
	Microsoft::WRL::ComPtr<ID3DBlob> converterShaderBlob_;
};
