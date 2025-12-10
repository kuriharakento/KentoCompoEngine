#pragma once
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
	static GPUParticlePipeline* GetInstance();

	void Initialize(DirectXCommon* dxCommon);
	void Finalize();

	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
	ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }
	ID3D12RootSignature* GetConverterRootSignature() const { return converterRootSignature_.Get(); }
	ID3D12PipelineState* GetConverterPipelineState() const { return converterPipelineState_.Get(); }
	bool IsValid() const { return pipelineState_ != nullptr; }

private:
	GPUParticlePipeline() = default;
	~GPUParticlePipeline() = default;
	GPUParticlePipeline(const GPUParticlePipeline&) = delete;
	GPUParticlePipeline& operator=(const GPUParticlePipeline&) = delete;

	void CompileShader();
	void CreateRootSignature();
	void CreatePipelineState();

	void CompileConverterShader();
	void CreateConverterRootSignature();
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
