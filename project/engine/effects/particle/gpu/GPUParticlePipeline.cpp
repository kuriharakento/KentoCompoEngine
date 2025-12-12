#include "GPUParticlePipeline.h"
#include "base/DirectXCommon.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

GPUParticlePipeline* GPUParticlePipeline::instance_ = nullptr;

GPUParticlePipeline* GPUParticlePipeline::GetInstance()
{
	if (!instance_)
	{
		instance_ = new GPUParticlePipeline();
	}
	return instance_;
}

void GPUParticlePipeline::Initialize(DirectXCommon* dxCommon)
{
	if (pipelineState_) return; // 既に初期化済み

	dxCommon_ = dxCommon;
	CreateRootSignature();
	CompileShader();
	CreatePipelineState();

	CreateConverterRootSignature();
	CompileConverterShader();
	CreateConverterPipelineState();
}

void GPUParticlePipeline::Finalize()
{
	pipelineState_.Reset();
	rootSignature_.Reset();
	shaderBlob_.Reset();

	converterPipelineState_.Reset();
	converterRootSignature_.Reset();
	converterShaderBlob_.Reset();

	delete instance_;
	instance_ = nullptr;
}

void GPUParticlePipeline::CreateRootSignature()
{
	auto* device = dxCommon_->GetDevice();

	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 1;
	uavRange.BaseShaderRegister = 0;
	uavRange.RegisterSpace = 0;
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParams[2]{};

	// CBV（定数バッファ）
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0;
	rootParams[0].Descriptor.RegisterSpace = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// UAV（パーティクルバッファ）
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = 2;
	desc.pParameters = rootParams;
	desc.NumStaticSamplers = 0;
	desc.pStaticSamplers = nullptr;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

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

	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

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
	if (!shaderBlob_ || !rootSignature_) return;

	auto* device = dxCommon_->GetDevice();

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.CS.pShaderBytecode = shaderBlob_->GetBufferPointer();
	psoDesc.CS.BytecodeLength = shaderBlob_->GetBufferSize();
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create compute pipeline state\n");
	}
}

void GPUParticlePipeline::CompileConverterShader()
{
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	UINT compileFlags = 0;
#ifdef _DEBUG
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

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

	D3D12_DESCRIPTOR_RANGE ranges[2]{};
	// SRV (Input) t0
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// UAV (Output) u0
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 0;
	ranges[1].RegisterSpace = 0;
	ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParams[4]{};

	// CBV (Constants) b0
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0;
	rootParams[0].Descriptor.RegisterSpace = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// CBV (Camera) b1
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[1].Descriptor.ShaderRegister = 1;
	rootParams[1].Descriptor.RegisterSpace = 0;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// SRV Table (Input)
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[2].DescriptorTable.pDescriptorRanges = &ranges[0];
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// UAV Table (Output)
	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[3].DescriptorTable.pDescriptorRanges = &ranges[1];
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = 4;
	desc.pParameters = rootParams;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

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
	if (!converterShaderBlob_ || !converterRootSignature_) return;

	auto* device = dxCommon_->GetDevice();

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = converterRootSignature_.Get();
	psoDesc.CS.pShaderBytecode = converterShaderBlob_->GetBufferPointer();
	psoDesc.CS.BytecodeLength = converterShaderBlob_->GetBufferSize();
	psoDesc.NodeMask = 0;
	psoDesc.CachedPSO.pCachedBlob = nullptr;
	psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&converterPipelineState_));
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create converter compute pipeline state\n");
	}
}
