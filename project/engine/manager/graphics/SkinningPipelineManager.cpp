#include "SkinningPipelineManager.h"
#include <cassert>

SkinningPipelineManager* SkinningPipelineManager::GetInstance()
{
	static SkinningPipelineManager instance;
	return &instance;
}

void SkinningPipelineManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	CreatePipeline();
}

void SkinningPipelineManager::Finalize()
{
	rootSignature_.Reset();
	pipelineState_.Reset();
}

void SkinningPipelineManager::CreatePipeline()
{
	HRESULT hr;
	ID3D12Device* device = dxCommon_->GetDevice();

	//----- ルートシグネチャの生成 -----//

	// ディスクリプタレンジ
	D3D12_DESCRIPTOR_RANGE ranges[3] = {};
	
	// t0: ボーン行列
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].NumDescriptors = 1;
	ranges[0].BaseShaderRegister = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// t1: 入力頂点
	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[1].NumDescriptors = 1;
	ranges[1].BaseShaderRegister = 1;
	ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// u0: 出力頂点
	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	ranges[2].NumDescriptors = 1;
	ranges[2].BaseShaderRegister = 0;
	ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータ
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &ranges[0];

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &ranges[1];

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &ranges[2];

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].Descriptor.ShaderRegister = 0;

	// シリアライズして生成
	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = _countof(rootParameters);
	rsDesc.pParameters = rootParameters;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	//----- パイプラインステートの生成 -----//

	auto csBlob = dxCommon_->CompileSharder(L"Resources/shaders/Skinning.CS.hlsl", L"cs_6_0");
	assert(csBlob && "Failed to compile CS");

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

	hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}
