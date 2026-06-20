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
#include <memory>

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
	 * @return GPUParticlePipelineのインスタンス
	 */
	static GPUParticlePipeline* GetInstance();

	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonポインタ
	 */
	void Initialize(DirectXCommon* dxCommon);

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief ルートシグネチャを取得
	 * @return ルートシグネチャ
	 */
	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

	/**
	 * @brief パイプラインステートを取得
	 * @return パイプラインステート
	 */
	ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

	/**
	 * @brief コンバーター用ルートシグネチャを取得
	 * @return コンバーター用ルートシグネチャ
	 */
	ID3D12RootSignature* GetConverterRootSignature() const { return converterRootSignature_.Get(); }

	/**
	 * @brief コンバーター用パイプラインステートを取得
	 * @return コンバーター用パイプラインステート
	 */
	ID3D12PipelineState* GetConverterPipelineState() const { return converterPipelineState_.Get(); }

	/**
	 * @brief 渦モジュール用ルートシグネチャを取得
	 */
	ID3D12RootSignature* GetVortexRootSignature() const { return vortexRootSignature_.Get(); }

	/**
	 * @brief 渦モジュール用パイプラインステートを取得
	 */
	ID3D12PipelineState* GetVortexPipelineState() const { return vortexPipelineState_.Get(); }

	/**
	 * @brief 引力モジュール用ルートシグネチャを取得
	 */
	ID3D12RootSignature* GetAttractorRootSignature() const { return attractorRootSignature_.Get(); }

	/**
	 * @brief 引力モジュール用パイプラインステートを取得
	 */
	ID3D12PipelineState* GetAttractorPipelineState() const { return attractorPipelineState_.Get(); }

	/**
	 * @brief カールノイズモジュール用ルートシグネチャを取得
	 */
	ID3D12RootSignature* GetCurlNoiseRootSignature() const { return curlNoiseRootSignature_.Get(); }

	/**
	 * @brief カールノイズモジュール用パイプラインステートを取得
	 */
	ID3D12PipelineState* GetCurlNoisePipelineState() const { return curlNoisePipelineState_.Get(); }

	/**
	 * @brief パイプラインが有効か判定
	 * @return 有効な場合true
	 */
	bool IsValid() const { return pipelineState_ != nullptr; }

private:
	GPUParticlePipeline() = default;
	GPUParticlePipeline(const GPUParticlePipeline&) = delete;
	GPUParticlePipeline& operator=(const GPUParticlePipeline&) = delete;

	/**
	 * @brief シミュレーション用コンピュートシェーダーをコンパイル
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
	 * @brief レンダリング変換用コンピュートシェーダーをコンパイル
	 */
	void CompileConverterShader();

	/**
	 * @brief レンダリング変換用ルートシグネチャを作成
	 */
	void CreateConverterRootSignature();

	/**
	 * @brief レンダリング変換用パイプラインステートを作成
	 */
	void CreateConverterPipelineState();

	/**
	 * @brief 各モジュール用のパイプラインを構築
	 */
	void CreateModulePipelines();

private:
	static std::unique_ptr<GPUParticlePipeline> instance_;

	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> converterRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> converterPipelineState_;
	Microsoft::WRL::ComPtr<ID3DBlob> converterShaderBlob_;

	// Vortex
	Microsoft::WRL::ComPtr<ID3D12RootSignature> vortexRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> vortexPipelineState_;
	Microsoft::WRL::ComPtr<ID3DBlob> vortexShaderBlob_;

	// Attractor
	Microsoft::WRL::ComPtr<ID3D12RootSignature> attractorRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> attractorPipelineState_;
	Microsoft::WRL::ComPtr<ID3DBlob> attractorShaderBlob_;

	// CurlNoise
	Microsoft::WRL::ComPtr<ID3D12RootSignature> curlNoiseRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> curlNoisePipelineState_;
	Microsoft::WRL::ComPtr<ID3DBlob> curlNoiseShaderBlob_;

public:
	~GPUParticlePipeline() = default;
};
