#include "GPUParticlePipeline.h"
#include "base/DirectXCommon.h"
#include <d3dcompiler.h>
#include <filesystem>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	// ルートパラメータ数
	constexpr uint32_t kSimulationRootParamCount = 2;
	constexpr uint32_t kConverterRootParamCount = 7;
	
	// デスクリプタレンジ数
	constexpr uint32_t kSimulationDescriptorRangeCount = 1;
	constexpr uint32_t kConverterDescriptorRangeCount = 2;
}

std::unique_ptr<GPUParticlePipeline> GPUParticlePipeline::instance_ = nullptr;

GPUParticlePipeline* GPUParticlePipeline::GetInstance()
{
	// シングルトンインスタンス生成
	if (!instance_)
	{
		instance_.reset(new GPUParticlePipeline());
	}
	return instance_.get();
}

void GPUParticlePipeline::Initialize(DirectXCommon* dxCommon)
{
	// 既に初期化済みの場合は何もしない
	if (pipelineState_) return;

	dxCommon_ = dxCommon;

	// シミュレーション用パイプラインを構築
	CreateRootSignature();
	CompileShader();
	CreatePipelineState();

	// レンダリング変換用パイプラインを構築
	CreateConverterRootSignature();
	CompileConverterShader();
	CreateConverterPipelineState();

	// WriteIndirectArgs用パイプラインを構築
	CreateWriteIndirectArgsRootSignature();
	CompileWriteIndirectArgsShader();
	CreateWriteIndirectArgsPipelineState();

	// コマンドシグネチャを作成
	CreateCommandSignature();

	// 各モジュール用パイプラインを構築
	CreateModulePipelines();
}

void GPUParticlePipeline::Finalize()
{
	// シミュレーション用リソースを解放
	pipelineState_.Reset();
	rootSignature_.Reset();
	shaderBlob_.Reset();

	// レンダリング変換用リソースを解放
	converterPipelineState_.Reset();
	converterRootSignature_.Reset();
	converterShaderBlob_.Reset();

	// WriteIndirectArgs用リソースを解放
	writeIndirectArgsPipelineState_.Reset();
	writeIndirectArgsRootSignature_.Reset();
	writeIndirectArgsShaderBlob_.Reset();

	// コマンドシグネチャを解放
	commandSignature_.Reset();

	// 各モジュール用リソースを解放
	vortexPipelineState_.Reset();
	vortexRootSignature_.Reset();
	vortexShaderBlob_.Reset();

	attractorPipelineState_.Reset();
	attractorRootSignature_.Reset();
	attractorShaderBlob_.Reset();

	curlNoisePipelineState_.Reset();
	curlNoiseRootSignature_.Reset();
	curlNoiseShaderBlob_.Reset();

	// シングルトンインスタンスを削除
	instance_.reset();
}

void GPUParticlePipeline::CreateRootSignature()
{
	auto* device = dxCommon_->GetDevice();

	// UAVディスクリプタレンジを定義（パーティクルバッファ用）
	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = kSimulationDescriptorRangeCount;
	uavRange.BaseShaderRegister = 0;
	uavRange.RegisterSpace = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータを定義（CBV + UAV）
	D3D12_ROOT_PARAMETER rootParams[2]{};

	// ルートパラメータ0: 定数バッファ（シミュレーションパラメータ）
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0;
	rootParams[0].Descriptor.RegisterSpace = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートパラメータ1: UAVディスクリプタテーブル（パーティクルバッファ）
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = kSimulationDescriptorRangeCount;
	rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートシグネチャをシリアライズして作成
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

	// デバッグビルド時のみデバッグ情報を含める
	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	// ファイルが存在しない場合の自動検索
	std::wstring filePath = L"Resources/shaders/ParticleCompute.hlsl";
	if (!std::filesystem::exists(filePath))
	{
		std::vector<std::wstring> searchPaths = {
			L"application/Resources/shaders/ParticleCompute.hlsl",
			L"../engine/Resources/shaders/ParticleCompute.hlsl",
			L"Resources/shaders/ParticleCompute.hlsl"
		};
		for (const auto& path : searchPaths)
		{
			if (std::filesystem::exists(path))
			{
				filePath = path;
				break;
			}
		}
	}

	// コンピュートシェーダーをコンパイル
	HRESULT hr = D3DCompileFromFile(
		filePath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"CSMain",
		"cs_5_0",
		compileFlags,
		0,
		&shaderBlob_,
		&errorBlob
	);

	// コンパイルエラーの場合はログ出力
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
	// シェーダーまたはルートシグネチャが無効な場合は作成しない
	if (!shaderBlob_ || !rootSignature_) return;

	auto* device = dxCommon_->GetDevice();

	// コンピュートパイプラインステートを設定
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.CS.pShaderBytecode = shaderBlob_->GetBufferPointer();
	psoDesc.CS.BytecodeLength = shaderBlob_->GetBufferSize();
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// パイプラインステートオブジェクトを作成
	HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create compute pipeline state\n");
	}
}

void GPUParticlePipeline::CompileConverterShader()
{
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	// デバッグビルド時のみデバッグ情報を含める
	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	// ファイルが存在しない場合の自動検索
	std::wstring filePath = L"Resources/shaders/ParticleConvert.CS.hlsl";
	if (!std::filesystem::exists(filePath))
	{
		std::vector<std::wstring> searchPaths = {
			L"application/Resources/shaders/ParticleConvert.CS.hlsl",
			L"../engine/Resources/shaders/ParticleConvert.CS.hlsl",
			L"Resources/shaders/ParticleConvert.CS.hlsl"
		};
		for (const auto& path : searchPaths)
		{
			if (std::filesystem::exists(path))
			{
				filePath = path;
				break;
			}
		}
	}

	// パーティクル→レンダリングデータ変換用コンピュートシェーダーをコンパイル
	HRESULT hr = D3DCompileFromFile(
		filePath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"CSMain",
		"cs_5_0",
		compileFlags,
		0,
		&converterShaderBlob_,
		&errorBlob
	);

	// コンパイルエラーの場合はログ出力
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

	// 5つのディスクリプタレンジを定義 (2つの入力SRV, 3つの出力UAV)
	D3D12_DESCRIPTOR_RANGE ranges[5]{};

	// 0: 入力: パーティクルバッファ (SRV t0)
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 1: 入力: 新規発生バッファ (SRV t1)
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 1;
	ranges[1].RegisterSpace = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 2: 出力: レンダリング用バッファ (UAV u0)
	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[2].NumDescriptors = 1;
	ranges[2].BaseShaderRegister = 0;
	ranges[2].RegisterSpace = 0;
	ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 3: 出力: 次フレームパーティクルバッファ (UAV u1)
	ranges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[3].NumDescriptors = 1;
	ranges[3].BaseShaderRegister = 1;
	ranges[3].RegisterSpace = 0;
	ranges[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 4: 出力: カウンタバッファ (UAV u2)
	ranges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[4].NumDescriptors = 1;
	ranges[4].BaseShaderRegister = 2;
	ranges[4].RegisterSpace = 0;
	ranges[4].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータを定義 (CBV×2 + SRV×2 + UAV×3 = 7)
	D3D12_ROOT_PARAMETER rootParams[7]{};

	// 0: 定数バッファ (b0)
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0;
	rootParams[0].Descriptor.RegisterSpace = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 1: カメラ定数 (b1)
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[1].Descriptor.ShaderRegister = 1;
	rootParams[1].Descriptor.RegisterSpace = 0;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 2: SRV t0 (gParticles)
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[2].DescriptorTable.pDescriptorRanges = &ranges[0];
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 3: SRV t1 (gSpawnParticles)
	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[3].DescriptorTable.pDescriptorRanges = &ranges[1];
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 4: UAV u0 (gRenderParticles)
	rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[4].DescriptorTable.pDescriptorRanges = &ranges[2];
	rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 5: UAV u1 (gOutParticles)
	rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[5].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[5].DescriptorTable.pDescriptorRanges = &ranges[3];
	rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 6: UAV u2 (gCounter)
	rootParams[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[6].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[6].DescriptorTable.pDescriptorRanges = &ranges[4];
	rootParams[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートシグネチャをシリアライズして作成
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
	// シェーダーまたはルートシグネチャが無効な場合は作成しない
	if (!converterShaderBlob_ || !converterRootSignature_) return;

	auto* device = dxCommon_->GetDevice();

	// コンバーター用コンピュートパイプラインステートを設定
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = converterRootSignature_.Get();
	psoDesc.CS.pShaderBytecode = converterShaderBlob_->GetBufferPointer();
	psoDesc.CS.BytecodeLength = converterShaderBlob_->GetBufferSize();
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// パイプラインステートオブジェクトを作成
	HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&converterPipelineState_));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create converter compute pipeline state\n");
	}
}

void GPUParticlePipeline::CreateModulePipelines()
{
	auto* device = dxCommon_->GetDevice();
	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	// パス検索用ヘルパー
	auto ResolvePath = [](const std::wstring& baseName) -> std::wstring
	{
		std::wstring filePath = L"Resources/shaders/" + baseName;
		if (!std::filesystem::exists(filePath))
		{
			std::vector<std::wstring> searchPaths = {
				L"application/Resources/shaders/" + baseName,
				L"../engine/Resources/shaders/" + baseName,
				L"Resources/shaders/" + baseName
			};
			for (const auto& path : searchPaths)
			{
				if (std::filesystem::exists(path))
				{
					filePath = path;
					break;
				}
			}
		}
		return filePath;
	};

	// Vortex
	{
		vortexRootSignature_ = rootSignature_; // ルートシグネチャは共有
		std::wstring filePath = ResolvePath(L"ParticleVortex.CS.hlsl");
		
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3DCompileFromFile(
			filePath.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"CSMain",
			"cs_5_0",
			compileFlags,
			0,
			&vortexShaderBlob_,
			&errorBlob
		);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
			}
		}
		assert(SUCCEEDED(hr));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = vortexRootSignature_.Get();
		psoDesc.CS = { vortexShaderBlob_->GetBufferPointer(), vortexShaderBlob_->GetBufferSize() };
		hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&vortexPipelineState_));
		assert(SUCCEEDED(hr));
	}

	// Attractor
	{
		attractorRootSignature_ = rootSignature_; // ルートシグネチャは共有
		std::wstring filePath = ResolvePath(L"ParticleAttractor.CS.hlsl");
		
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3DCompileFromFile(
			filePath.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"CSMain",
			"cs_5_0",
			compileFlags,
			0,
			&attractorShaderBlob_,
			&errorBlob
		);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
			}
		}
		assert(SUCCEEDED(hr));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = attractorRootSignature_.Get();
		psoDesc.CS = { attractorShaderBlob_->GetBufferPointer(), attractorShaderBlob_->GetBufferSize() };
		hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&attractorPipelineState_));
		assert(SUCCEEDED(hr));
	}

	// CurlNoise
	{
		curlNoiseRootSignature_ = rootSignature_; // ルートシグネチャは共有
		std::wstring filePath = ResolvePath(L"ParticleCurlNoise.CS.hlsl");
		
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3DCompileFromFile(
			filePath.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"CSMain",
			"cs_5_0",
			compileFlags,
			0,
			&curlNoiseShaderBlob_,
			&errorBlob
		);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
			}
		}
		assert(SUCCEEDED(hr));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = curlNoiseRootSignature_.Get();
		psoDesc.CS = { curlNoiseShaderBlob_->GetBufferPointer(), curlNoiseShaderBlob_->GetBufferSize() };
		hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&curlNoisePipelineState_));
		assert(SUCCEEDED(hr));
	}
}

void GPUParticlePipeline::CreateWriteIndirectArgsRootSignature()
{
	auto* device = dxCommon_->GetDevice();

	D3D12_DESCRIPTOR_RANGE ranges[2]{};

	// 入力: 生存数カウンタ (SRV t0)
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 出力: 間接描画引数バッファ (UAV u0)
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].RegisterSpace = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParams[2]{};

	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[0].DescriptorTable.pDescriptorRanges = &ranges[0];
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[1].DescriptorTable.pDescriptorRanges = &ranges[1];
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = 2;
	desc.pParameters = rootParams;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (SUCCEEDED(hr))
	{
		device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&writeIndirectArgsRootSignature_));
	}
}

void GPUParticlePipeline::CompileWriteIndirectArgsShader()
{
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	std::wstring filePath = L"Resources/shaders/WriteIndirectArgs.CS.hlsl";
	if (!std::filesystem::exists(filePath))
	{
		std::vector<std::wstring> searchPaths = {
			L"application/Resources/shaders/WriteIndirectArgs.CS.hlsl",
			L"../engine/Resources/shaders/WriteIndirectArgs.CS.hlsl",
			L"Resources/shaders/WriteIndirectArgs.CS.hlsl"
		};
		for (const auto& path : searchPaths)
		{
			if (std::filesystem::exists(path))
			{
				filePath = path;
				break;
			}
		}
	}

	HRESULT hr = D3DCompileFromFile(
		filePath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"CSMain",
		"cs_5_0",
		compileFlags,
		0,
		&writeIndirectArgsShaderBlob_,
		&errorBlob
	);

	if (FAILED(hr))
	{
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}
	}
}

void GPUParticlePipeline::CreateWriteIndirectArgsPipelineState()
{
	if (!writeIndirectArgsShaderBlob_ || !writeIndirectArgsRootSignature_) return;

	auto* device = dxCommon_->GetDevice();

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = writeIndirectArgsRootSignature_.Get();
	psoDesc.CS.pShaderBytecode = writeIndirectArgsShaderBlob_->GetBufferPointer();
	psoDesc.CS.BytecodeLength = writeIndirectArgsShaderBlob_->GetBufferSize();
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&writeIndirectArgsPipelineState_));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create write indirect args compute pipeline state\n");
	}
}

void GPUParticlePipeline::CreateCommandSignature()
{
	auto* device = dxCommon_->GetDevice();

	// Draw用
	{
		D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
		argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

		D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
		sigDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
		sigDesc.NumArgumentDescs = 1;
		sigDesc.pArgumentDescs = &argDesc;
		sigDesc.NodeMask = 0;

		HRESULT hr = device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&commandSignature_));
		if (FAILED(hr))
		{
			OutputDebugStringA("Failed to create command signature\n");
		}
	}

	// DrawIndexed用
	{
		D3D12_INDIRECT_ARGUMENT_DESC argDesc{};
		argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC sigDesc{};
		sigDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
		sigDesc.NumArgumentDescs = 1;
		sigDesc.pArgumentDescs = &argDesc;
		sigDesc.NodeMask = 0;

		HRESULT hr = device->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&meshCommandSignature_));
		if (FAILED(hr))
		{
			OutputDebugStringA("Failed to create mesh command signature\n");
		}
	}
}
