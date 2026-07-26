#include "SkinningCompute.h"

#include <cassert>
#include <algorithm>
#include <d3dcompiler.h>

#include "manager/system/SrvManager.h"
#include "manager/graphics/SkinningPipelineManager.h"

namespace KCE
{
// スレッドグループサイズ
constexpr UINT kThreadGroupSize = 256;

SkinningCompute::~SkinningCompute()
{
	// 確保したSRV/UAVを解放する
	if (srvManager_)
	{
		if (boneMatrixSrvIndex_ != SrvManager::kInvalidSrvIndex) srvManager_->Free(boneMatrixSrvIndex_);
		if (inputSrvIndex_      != SrvManager::kInvalidSrvIndex) srvManager_->Free(inputSrvIndex_);
		if (outputSrvIndex_     != SrvManager::kInvalidSrvIndex) srvManager_->Free(outputSrvIndex_);
		if (outputUavIndex_     != SrvManager::kInvalidSrvIndex) srvManager_->Free(outputUavIndex_);
	}
}

void SkinningCompute::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

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

	// SRV/UAVのインデックスを確保（初回のみ）
	if (inputSrvIndex_ == SrvManager::kInvalidSrvIndex)
	{
		inputSrvIndex_ = srvManager_->Allocate();
	}
	if (outputUavIndex_ == SrvManager::kInvalidSrvIndex)
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
	auto* pipelineManager = SkinningPipelineManager::GetInstance();

	// 出力バッファをUAV状態に遷移
	D3D12_RESOURCE_BARRIER toUav = {};
	toUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toUav.Transition.pResource = outputBuffer_;
	toUav.Transition.StateBefore = currentState;
	toUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &toUav);

	// コンピュートパイプラインを設定
	commandList->SetComputeRootSignature(pipelineManager->GetRootSignature());
	commandList->SetPipelineState(pipelineManager->GetPipelineState());

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
} // namespace KCE
