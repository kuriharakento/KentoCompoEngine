#include "SkinningCompute.h"

#include <cassert>
#include <algorithm>
#include <d3dcompiler.h>

#include "manager/system/SrvManager.h"

// スレッドグループサイズ
constexpr UINT kThreadGroupSize = 256;

void SkinningCompute::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	CreateRootSignature();
	CreatePipelineState();
	CreateBoneMatrixBuffer();
	CreateConstantBuffer();
}

void SkinningCompute::PrepareResources(uint32_t vertexCount, ID3D12Resource* inputBuffer, ID3D12Resource* outputBuffer)
{
	currentVertexCount_ = vertexCount;
	inputBuffer_ = inputBuffer;
	outputBuffer_ = outputBuffer;

	// 定数バッファを更新
	constantData_->vertexCount = vertexCount;

	// SRV/UAVのインデックスを確保（必要に応じて）
	if (inputSrvIndex_ == 0)
	{
		inputSrvIndex_ = srvManager_->Allocate();
	}
	if (outputUavIndex_ == 0)
	{
		outputUavIndex_ = srvManager_->Allocate();
	}

	// 入力バッファのSRVを作成 (ByteAddressBuffer / RAW として作成)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = static_cast<UINT>(vertexCount * sizeof(SkinnedVertexData) / 4); // DWORD単位
	srvDesc.Buffer.StructureByteStride = 0; // RAWバッファは0
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	dxCommon_->GetDevice()->CreateShaderResourceView(
		inputBuffer_,
		&srvDesc,
		srvManager_->GetCPUDescriptorHandle(inputSrvIndex_)
	);

	// 出力バッファのUAVを作成
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = vertexCount;
	uavDesc.Buffer.StructureByteStride = sizeof(VertexData);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	dxCommon_->GetDevice()->CreateUnorderedAccessView(
		outputBuffer_,
		nullptr,
		&uavDesc,
		srvManager_->GetCPUDescriptorHandle(outputUavIndex_)
	);
}

void SkinningCompute::UpdateBoneMatrices(const std::vector<Matrix4x4>& boneMatrices)
{
	size_t copySize = (std::min)(boneMatrices.size(), static_cast<size_t>(kMaxBones)) * sizeof(Matrix4x4);
	std::memcpy(boneMatrixData_, boneMatrices.data(), copySize);
}

void SkinningCompute::Dispatch(D3D12_RESOURCE_STATES currentState)
{
	if (currentVertexCount_ == 0)
	{
		return;
	}

	auto* commandList = dxCommon_->GetCommandList();

	// 出力バッファをUAV状態に遷移
	D3D12_RESOURCE_BARRIER toUav = {};
	toUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toUav.Transition.pResource = outputBuffer_;
	toUav.Transition.StateBefore = currentState;
	toUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &toUav);

	// コンピュートパイプラインを設定
	commandList->SetComputeRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvManager_->GetSrvHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	// ボーン行列バッファをバインド（t0）
	commandList->SetComputeRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(boneMatrixSrvIndex_));

	// 入力頂点バッファをバインド（t1）
	commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(inputSrvIndex_));

	// 出力バッファをバインド（u0）
	commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(outputUavIndex_));

	// 定数バッファをバインド
	commandList->SetComputeRootConstantBufferView(3, constantBuffer_->GetGPUVirtualAddress());

	// ディスパッチ
	UINT threadGroupCount = (currentVertexCount_ + kThreadGroupSize - 1) / kThreadGroupSize;
	commandList->Dispatch(threadGroupCount, 1, 1);

	// UAVバリアを挿入
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = outputBuffer_;
	commandList->ResourceBarrier(1, &uavBarrier);

	// 出力バッファを頂点バッファ状態に遷移
	D3D12_RESOURCE_BARRIER toVb = {};
	toVb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toVb.Transition.pResource = outputBuffer_;
	toVb.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	toVb.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	toVb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &toVb);
}

void SkinningCompute::CreateRootSignature()
{
	HRESULT hr;

	// ディスクリプタレンジ
	D3D12_DESCRIPTOR_RANGE boneMatrixRange = {};
	boneMatrixRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	boneMatrixRange.NumDescriptors = 1;
	boneMatrixRange.BaseShaderRegister = 0; // t0
	boneMatrixRange.RegisterSpace = 0;
	boneMatrixRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE inputVertexRange = {};
	inputVertexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	inputVertexRange.NumDescriptors = 1;
	inputVertexRange.BaseShaderRegister = 1; // t1
	inputVertexRange.RegisterSpace = 0;
	inputVertexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE outputVertexRange = {};
	outputVertexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	outputVertexRange.NumDescriptors = 1;
	outputVertexRange.BaseShaderRegister = 0; // u0
	outputVertexRange.RegisterSpace = 0;
	outputVertexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータ
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// t0: ボーン行列
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &boneMatrixRange;

	// t1: 入力頂点
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &inputVertexRange;

	// u0: 出力頂点
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &outputVertexRange;

	// b0: 定数バッファ
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[3].Descriptor.ShaderRegister = 0;
	rootParameters[3].Descriptor.RegisterSpace = 0;

	// ルートシグネチャの作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_)
	);
	assert(SUCCEEDED(hr));
}

void SkinningCompute::CreatePipelineState()
{
	HRESULT hr;

	// コンピュートシェーダーをコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileSharder(
		L"Resources/shaders/Skinning.CS.hlsl", L"cs_6_0"
	);
	assert(computeShaderBlob != nullptr);

	// パイプラインステートの作成
	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc = {};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void SkinningCompute::CreateBoneMatrixBuffer()
{
	// ボーン行列バッファを作成
	size_t bufferSize = sizeof(Matrix4x4) * kMaxBones;
	boneMatrixBuffer_ = dxCommon_->CreateBufferResource(bufferSize);

	// マッピング
	boneMatrixBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&boneMatrixData_));

	// 単位行列で初期化
	for (uint32_t i = 0; i < kMaxBones; ++i)
	{
		boneMatrixData_[i] = MakeIdentity4x4();
	}

	// SRVを作成
	boneMatrixSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(
		boneMatrixSrvIndex_,
		boneMatrixBuffer_.Get(),
		kMaxBones,
		sizeof(Matrix4x4)
	);
}

void SkinningCompute::CreateConstantBuffer()
{
	constantBuffer_ = dxCommon_->CreateBufferResource(sizeof(SkinningConstants));
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
	constantData_->vertexCount = 0;
	constantData_->padding[0] = 0;
	constantData_->padding[1] = 0;
	constantData_->padding[2] = 0;
}
