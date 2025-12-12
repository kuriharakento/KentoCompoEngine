#include "GPUParticlePipeline.h"
#include "base/DirectXCommon.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	// ルートパラメータ数
	constexpr uint32_t kSimulationRootParamCount = 2;
	constexpr uint32_t kConverterRootParamCount = 4;
	
	// デスクリプタレンジ数
	constexpr uint32_t kSimulationDescriptorRangeCount = 1;
	constexpr uint32_t kConverterDescriptorRangeCount = 2;
}

GPUParticlePipeline* GPUParticlePipeline::instance_ = nullptr;

GPUParticlePipeline* GPUParticlePipeline::GetInstance()
{
	// シングルトンインスタンス生成
	if (!instance_)
	{
		instance_ = new GPUParticlePipeline();
	}
	return instance_;
}

void GPUParticlePipeline::Initialize(DirectXCommon* dxCommon)
{
	// 既に初期化済みの場合は処理をスキップ
	if (pipelineState_) return;

	dxCommon_ = dxCommon;
	
	// シミュレーション用パイプラインの構築
	CreateRootSignature();
	CompileShader();
	CreatePipelineState();

	// コンバーター用パイプラインの構築
	CreateConverterRootSignature();
	CompileConverterShader();
	CreateConverterPipelineState();
}

void GPUParticlePipeline::Finalize()
{
	// シミュレーション用リソースの解放
	pipelineState_.Reset();
	rootSignature_.Reset();
	shaderBlob_.Reset();

	// コンバーター用リソースの解放
	converterPipelineState_.Reset();
	converterRootSignature_.Reset();
	converterShaderBlob_.Reset();

	// シングルトンインスタンスの削除
	delete instance_;
	instance_ = nullptr;
}

void GPUParticlePipeline::CreateRootSignature()
{
	auto* device = dxCommon_->GetDevice();

	// UAVレンジの設定（パーティクルバッファ）
	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = kSimulationDescriptorRangeCount;
	uavRange.BaseShaderRegister = 0;
	uavRange.RegisterSpace = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParams[kSimulationRootParamCount]{};

	// CBV（定数バッファ b0）
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0;
	rootParams[0].Descriptor.RegisterSpace = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// UAV（パーティクルバッファ u0）
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = kSimulationDescriptorRangeCount;
	rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートシグネチャデスクの構築
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = kSimulationRootParamCount;
	desc.pParameters = rootParams;
	desc.NumStaticSamplers = 0;
	desc.pStaticSamplers = nullptr;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	// シリアライズと作成
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (SUCCEEDED(hr))
	{
		device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	}
}

void GPUParticlePipeline::CompileShader()
{
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	// デバッグビルドではシェーダーのデバッグ情報を含める
	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	// パーティクルシミュレーション用コンピュートシェーダーをコンパイル
	HRESULT hr = D3DCompileFromFile(
		L"Resources/shaders/ParticleCompute.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"CSMain",
		"cs_5_0",
		compileFlags,
		0,
		&shaderBlob_,
		&errorBlob
	);

	// コンパイルエラー時はデバッグ出力
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}
	}
}

void GPUParticlePipeline::CreatePipelineState()
{
	// シェーダーまたはルートシグネチャが未作成の場合は処理をスキップ
	if (!shaderBlob_ || !rootSignature_) return;

	auto* device = dxCommon_->GetDevice();

	// コンピュートパイプラインステートデスクの構築
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.CS.pShaderBytecode = shaderBlob_->GetBufferPointer();
	psoDesc.CS.BytecodeLength = shaderBlob_->GetBufferSize();
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// パイプラインステートの作成
	HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create compute pipeline state\n");
	}
}

void GPUParticlePipeline::CompileConverterShader()
{
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	// デバッグビルドではシェーダーのデバッグ情報を含める
	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	// パーティクル変換用コンピュートシェーダーをコンパイル
	HRESULT hr = D3DCompileFromFile(
		L"Resources/shaders/ParticleConvert.CS.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"CSMain",
		"cs_5_0",
		compileFlags,
		0,
		&converterShaderBlob_,
		&errorBlob
	);

	// コンパイルエラー時はデバッグ出力
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}
	}
}

void GPUParticlePipeline::CreateConverterRootSignature()
{
	auto* device = dxCommon_->GetDevice();

	// デスクリプタレンジの設定（入力SRVと出力UAV）
	D3D12_DESCRIPTOR_RANGE ranges[kConverterDescriptorRangeCount]{};
	
	// SRV (Input) t0 - パーティクルバッファ読み取り用
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// UAV (Output) u0 - レンダリングバッファ書き込み用
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].RegisterSpace = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParams[kConverterRootParamCount]{};

	// CBV (Constants) b0 - パーティクル定数バッファ
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0;
	rootParams[0].Descriptor.RegisterSpace = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// CBV (Camera) b1 - カメラ定数バッファ
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[1].Descriptor.ShaderRegister = 1;
	rootParams[1].Descriptor.RegisterSpace = 0;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// SRV Table (Input) - パーティクルバッファ
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[2].DescriptorTable.pDescriptorRanges = &ranges[0];
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// UAV Table (Output) - レンダリングバッファ
	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[3].DescriptorTable.pDescriptorRanges = &ranges[1];
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートシグネチャデスクの構築
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = kConverterRootParamCount;
	desc.pParameters = rootParams;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	// シリアライズと作成
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (SUCCEEDED(hr))
	{
		device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&converterRootSignature_));
	}
}

void GPUParticlePipeline::CreateConverterPipelineState()
{
	// シェーダーまたはルートシグネチャが未作成の場合は処理をスキップ
	if (!converterShaderBlob_ || !converterRootSignature_) return;

	auto* device = dxCommon_->GetDevice();

	// コンピュートパイプラインステートデスクの構築
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = converterRootSignature_.Get();
	psoDesc.CS.pShaderBytecode = converterShaderBlob_->GetBufferPointer();
	psoDesc.CS.BytecodeLength = converterShaderBlob_->GetBufferSize();
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// パイプラインステートの作成
	HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&converterPipelineState_));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create converter compute pipeline state\n");
	}
}
