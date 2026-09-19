#include "GPUParticlePipeline.h"
#include "base/DirectXCommon.h"
#include <d3dcompiler.h>
#include <filesystem>
#include <vector>

namespace KCE
{
#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	// ルートパラメータ数
	constexpr uint32_t kSimulationRootParamCount = 10;
	constexpr uint32_t kConverterRootParamCount = 5;
	
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
		instance_ = std::make_unique<GPUParticlePipeline>();
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
	CompileSpawnPrepareShader();
	CreateSpawnPreparePipelineState();
	CreateRibbonComputePipeline();

	// レンダリング変換用パイプラインを構築
	CreateConverterRootSignature();
	CompileConverterShader();
	CreateConverterPipelineState();

	// 各モジュール用パイプラインを構築
	CreateModulePipelines();
}

void GPUParticlePipeline::Finalize()
{
	// シミュレーション用リソースを解放
	pipelineState_.Reset();
	rootSignature_.Reset();
	shaderBlob_.Reset();
	spawnPreparePipelineState_.Reset();
	spawnPrepareShaderBlob_.Reset();
	ribbonComputeRootSignature_.Reset();
	ribbonPrefixPipelineState_.Reset();
	ribbonScanPipelineState_.Reset();
	ribbonEmitPipelineState_.Reset();

	// レンダリング変換用リソースを解放
	converterPipelineState_.Reset();
	converterRootSignature_.Reset();
	converterShaderBlob_.Reset();

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
	D3D12_DESCRIPTOR_RANGE counterRange{};
	counterRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	counterRange.NumDescriptors = 1;
	counterRange.BaseShaderRegister = 1;
	counterRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE emitterStateRange{};
	emitterStateRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	emitterStateRange.NumDescriptors = 1;
	emitterStateRange.BaseShaderRegister = 2;
	emitterStateRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE eventRange{};
	eventRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	eventRange.NumDescriptors = 1;
	eventRange.BaseShaderRegister = 3;
	eventRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_DESCRIPTOR_RANGE eventCounterRange{};
	eventCounterRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	eventCounterRange.NumDescriptors = 1;
	eventCounterRange.BaseShaderRegister = 4;
	eventCounterRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE sourceEventRange{};
	sourceEventRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	sourceEventRange.NumDescriptors = 1;
	sourceEventRange.BaseShaderRegister = 0;
	sourceEventRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_DESCRIPTOR_RANGE sourceCounterRange{};
	sourceCounterRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	sourceCounterRange.NumDescriptors = 1;
	sourceCounterRange.BaseShaderRegister = 1;
	sourceCounterRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_DESCRIPTOR_RANGE moduleProgramRange{};
	moduleProgramRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	moduleProgramRange.NumDescriptors = 1;
	moduleProgramRange.BaseShaderRegister = 2;
	moduleProgramRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_DESCRIPTOR_RANGE moduleLutRange{};
	moduleLutRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	moduleLutRange.NumDescriptors = 1;
	moduleLutRange.BaseShaderRegister = 3;
	moduleLutRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParams[10]{};

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

	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[2].DescriptorTable.pDescriptorRanges = &counterRange;
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[3].DescriptorTable.pDescriptorRanges = &emitterStateRange;
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[4].DescriptorTable.pDescriptorRanges = &eventRange;
	rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[5].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[5].DescriptorTable.pDescriptorRanges = &eventCounterRange;
	rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[6].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[6].DescriptorTable.pDescriptorRanges = &sourceEventRange;
	rootParams[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[7].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[7].DescriptorTable.pDescriptorRanges = &sourceCounterRange;
	rootParams[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[8].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[8].DescriptorTable.pDescriptorRanges = &moduleProgramRange;
	rootParams[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParams[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[9].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[9].DescriptorTable.pDescriptorRanges = &moduleLutRange;
	rootParams[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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
			L"engine/Resources/shaders/ParticleCompute.hlsl",
			L"Resources/shaders/ParticleCompute.hlsl",
			L"../Resources/shaders/ParticleCompute.hlsl"
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

void GPUParticlePipeline::CompileSpawnPrepareShader()
{
	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	std::wstring filePath = L"Resources/shaders/ParticleSpawnPrepare.CS.hlsl";
	if (!std::filesystem::exists(filePath))
	{
		for (const auto& candidate : { L"engine/Resources/shaders/ParticleSpawnPrepare.CS.hlsl", L"../Resources/shaders/ParticleSpawnPrepare.CS.hlsl" })
		{
			if (std::filesystem::exists(candidate)) { filePath = candidate; break; }
		}
	}
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3DCompileFromFile(filePath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"CSMain", "cs_5_0", compileFlags, 0, &spawnPrepareShaderBlob_, &errorBlob);
	if (FAILED(hr) && errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
}

void GPUParticlePipeline::CreateSpawnPreparePipelineState()
{
	if (!spawnPrepareShaderBlob_ || !rootSignature_) return;
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.CS = { spawnPrepareShaderBlob_->GetBufferPointer(), spawnPrepareShaderBlob_->GetBufferSize() };
	dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&spawnPreparePipelineState_));
}

void GPUParticlePipeline::CreateRibbonComputePipeline()
{
	D3D12_DESCRIPTOR_RANGE ranges[7]{};
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	for (uint32_t i = 0; i < 6; ++i)
	{
		ranges[i + 1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		ranges[i + 1].NumDescriptors = 1;
		ranges[i + 1].BaseShaderRegister = i;
		ranges[i + 1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}
	D3D12_ROOT_PARAMETER params[10]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[1].Descriptor.ShaderRegister = 1;
	for (uint32_t i = 0; i < 7; ++i)
	{
		params[i + 2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[i + 2].DescriptorTable.NumDescriptorRanges = 1;
		params[i + 2].DescriptorTable.pDescriptorRanges = &ranges[i];
	}
	D3D12_ROOT_SIGNATURE_DESC rootDesc{};
	params[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	params[9].Constants.ShaderRegister = 2;
	params[9].Constants.Num32BitValues = 3;
	rootDesc.NumParameters = 10;
	rootDesc.pParameters = params;
	Microsoft::WRL::ComPtr<ID3DBlob> signature;
	Microsoft::WRL::ComPtr<ID3DBlob> error;
	if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) return;
	if (FAILED(dxCommon_->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(&ribbonComputeRootSignature_)))) return;

	std::wstring path = L"Resources/shaders/ParticleRibbon.CS.hlsl";
	if (!std::filesystem::exists(path)) path = L"engine/Resources/shaders/ParticleRibbon.CS.hlsl";
	UINT flags = 0;
#ifdef _DEBUG
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	auto createPass = [&](const char* entry, Microsoft::WRL::ComPtr<ID3D12PipelineState>& output)
	{
		Microsoft::WRL::ComPtr<ID3DBlob> shader;
		Microsoft::WRL::ComPtr<ID3DBlob> compileError;
		if (FAILED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, "cs_5_0",
			flags, 0, &shader, &compileError)))
		{
			if (compileError) OutputDebugStringA(static_cast<const char*>(compileError->GetBufferPointer()));
			return;
		}
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = ribbonComputeRootSignature_.Get();
		desc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
		dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&output));
	};
	createPass("BuildPrefix", ribbonPrefixPipelineState_);
	createPass("ScanGroups", ribbonScanPipelineState_);
	createPass("InitializeSort", ribbonSortInitializePipelineState_);
	createPass("BitonicSort", ribbonSortPipelineState_);
	createPass("EmitVertices", ribbonEmitPipelineState_);
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
			L"engine/Resources/shaders/ParticleConvert.CS.hlsl",
			L"Resources/shaders/ParticleConvert.CS.hlsl",
			L"../Resources/shaders/ParticleConvert.CS.hlsl"
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

	// ディスクリプタレンジを定義（入力SRVと出力UAV）
	D3D12_DESCRIPTOR_RANGE ranges[3]{};

	// 入力: パーティクルバッファ（SRV t0）
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 出力: レンダリング用バッファ（UAV u0）
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].RegisterSpace = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[2].NumDescriptors = 1;
	ranges[2].BaseShaderRegister = 1;
	ranges[2].RegisterSpace = 0;
	ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータを定義（CBV×2 + SRV + UAV）
	D3D12_ROOT_PARAMETER rootParams[5]{};

	// ルートパラメータ0: 定数バッファ（パーティクルパラメータ b0）
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0;
	rootParams[0].Descriptor.RegisterSpace = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートパラメータ1: 定数バッファ（カメラ情報 b1）
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[1].Descriptor.ShaderRegister = 1;
	rootParams[1].Descriptor.RegisterSpace = 0;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートパラメータ2: SRVディスクリプタテーブル（入力バッファ）
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[2].DescriptorTable.pDescriptorRanges = &ranges[0];
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートパラメータ3: UAVディスクリプタテーブル（出力バッファ）
	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[3].DescriptorTable.pDescriptorRanges = &ranges[1];
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[4].DescriptorTable.pDescriptorRanges = &ranges[2];
	rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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
				L"engine/Resources/shaders/" + baseName,
				L"Resources/shaders/" + baseName,
				L"../Resources/shaders/" + baseName
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
} // namespace KCE
