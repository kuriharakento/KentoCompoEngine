#include "GPUSimulator.h"
#include "effects/particle/gpu/GPUParticlePipeline.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include <algorithm>
#include <DirectXTex/d3dx12.h>

namespace
{
	// 定数バッファのアライメント（256バイト境界）
	constexpr size_t kConstantBufferAlignment = 256;
	
	// デフォルト重力（Y軸下向き）
	constexpr float kDefaultGravityY = -9.8f;
	
	// パーティクル履歴保持最大数
	constexpr size_t kMaxParticleHistorySize = 4;
}

GPUSimulator::GPUSimulator() = default;

GPUSimulator::~GPUSimulator()
{
	// 定数バッファのアンマップ
	if (constantBuffer_)
	{
		constantBuffer_->Unmap(0, nullptr);
	}

	// SRV/UAVディスクリプタの解放
	if (srvManager_)
	{
		if (particleSrvIndex_ != 0) srvManager_->Free(particleSrvIndex_);
		if (particleUavIndex_ != 0) srvManager_->Free(particleUavIndex_);
		if (renderSrvIndex_ != 0) srvManager_->Free(renderSrvIndex_);
		if (renderUavIndex_ != 0) srvManager_->Free(renderUavIndex_);
	}
}

void GPUSimulator::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t maxParticles)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	maxParticles_ = maxParticles;

	// 共有パイプラインを初期化（既に初期化済みの場合はスキップ）
	GPUParticlePipeline::GetInstance()->Initialize(dxCommon);

	// エミッター専用バッファの作成
	CreateBuffers();

	initialized_ = true;
}

void GPUSimulator::CreateBuffers()
{
	auto* device = dxCommon_->GetDevice();

	// パーティクルバッファ（UAV、GPU上でシミュレーション）
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(Particle) * maxParticles_;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		// バッファはCOMMON状態で作成（D3D12仕様）
		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&particleBuffer_)
		);
	}

	// アップロードバッファ（CPU→GPU転送用）
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(Particle) * maxParticles_;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&particleUploadBuffer_)
		);
	}

	// リードバックバッファ（GPU→CPU読み取り用）
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(Particle) * maxParticles_;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&particleReadbackBuffer_)
		);
	}

	// 定数バッファ（256バイト境界にアライン）
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = (sizeof(GPUParticleConstants) + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBuffer_)
		);

		// CPU側から書き込み可能にするためマップ
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
	}



	// レンダリング用バッファ（変換後のParticleGPU構造体格納用）
	{
		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
			sizeof(ParticleGPU) * maxParticles_,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		);

		// 初期状態はCOMMON（SRVとして使う直前にバリア遷移）
		dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&renderBuffer_)
		);
	}

	// SRV/UAVディスクリプタの確保
	particleSrvIndex_ = srvManager_->Allocate();
	particleUavIndex_ = srvManager_->Allocate();
	renderSrvIndex_ = srvManager_->Allocate();
	renderUavIndex_ = srvManager_->Allocate();

	// パーティクルバッファのSRV作成（読み取り用）
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = maxParticles_;
		srvDesc.Buffer.StructureByteStride = sizeof(Particle);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		device->CreateShaderResourceView(
			particleBuffer_.Get(),
			&srvDesc,
			srvManager_->GetCPUDescriptorHandle(particleSrvIndex_)
		);
	}

	// パーティクルバッファのUAV作成（書き込み用）
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = maxParticles_;
		uavDesc.Buffer.StructureByteStride = sizeof(Particle);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->CreateUnorderedAccessView(
			particleBuffer_.Get(),
			nullptr,
			&uavDesc,
			srvManager_->GetCPUDescriptorHandle(particleUavIndex_)
		);
	}

	// レンダリングバッファのSRV作成（描画時の読み取り用）
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = maxParticles_;
		srvDesc.Buffer.StructureByteStride = sizeof(ParticleGPU);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		device->CreateShaderResourceView(
			renderBuffer_.Get(),
			&srvDesc,
			srvManager_->GetCPUDescriptorHandle(renderSrvIndex_)
		);
	}

	// レンダリングバッファのUAV作成（変換時の書き込み用）
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = maxParticles_;
		uavDesc.Buffer.StructureByteStride = sizeof(ParticleGPU);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->CreateUnorderedAccessView(
			renderBuffer_.Get(),
			nullptr,
			&uavDesc,
			srvManager_->GetCPUDescriptorHandle(renderUavIndex_)
		);
	}
}

void GPUSimulator::SpawnParticles(const std::vector<Particle>& newParticles)
{
	if (newParticles.empty()) return;

	// アップロードバッファにパーティクルデータをコピー
	void* data = nullptr;
	particleUploadBuffer_->Map(0, nullptr, &data);
	size_t copyCount = (std::min)(newParticles.size(), static_cast<size_t>(maxParticles_ - particleCount_));
	memcpy(static_cast<Particle*>(data) + particleCount_, newParticles.data(), sizeof(Particle) * copyCount);
	particleUploadBuffer_->Unmap(0, nullptr);

	auto* cmdList = dxCommon_->GetCommandList();

	// 現在の状態からCOPY_DESTへ遷移
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = particleBuffer_.Get();
	barrier.Transition.StateBefore = particleBufferState_;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdList->ResourceBarrier(1, &barrier);

	// GPUバッファへコピー
	cmdList->CopyBufferRegion(
		particleBuffer_.Get(),
		particleCount_ * sizeof(Particle),
		particleUploadBuffer_.Get(),
		particleCount_ * sizeof(Particle),
		copyCount * sizeof(Particle)
	);

	// COPY_DESTからUAVへ遷移
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	cmdList->ResourceBarrier(1, &barrier);

	// 状態とカウントを更新
	particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	particleCount_ += static_cast<uint32_t>(copyCount);
}

void GPUSimulator::UpdateConstantBuffer(float deltaTime)
{
	// 定数バッファにシミュレーションパラメータを書き込み
	if (constantData_)
	{
		constantData_->deltaTime = deltaTime;
		constantData_->totalTime = totalTime_;
		constantData_->particleCount = particleCount_;
		constantData_->maxParticles = maxParticles_;
		constantData_->emitterPosition = emitterPosition_;
		constantData_->gravity = gravity_;
		constantData_->isBillboard = isBillboard_ ? 1 : 0;
	}
}

void GPUSimulator::Dispatch(float deltaTime, CameraManager* camera)
{
	// 未初期化の場合は処理をスキップ
	if (!initialized_) return;

	// 経過時間を累積して定数バッファを更新
	totalTime_ += deltaTime;
	UpdateConstantBuffer(deltaTime);

	auto* commandList = dxCommon_->GetCommandList();
	auto* pipeline = GPUParticlePipeline::GetInstance();

	// パイプラインが無効な場合は処理をスキップ
	if (!pipeline->IsValid()) return;

	// パーティクルバッファをUAV状態にする
	if (particleBufferState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_.Get(),
			particleBufferState_,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		commandList->ResourceBarrier(1, &barrier);
		particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	// ========================================
	// フェーズ1: シミュレーション（パーティクルバッファ更新）
	// ========================================

	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = { srvManager_->GetSrvHeap() };
	commandList->SetDescriptorHeaps(1, heaps);

	// パイプラインとルートシグネチャを設定
	commandList->SetPipelineState(pipeline->GetPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetRootSignature());
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(particleUavIndex_));

	// コンピュートシェーダーをディスパッチ
	uint32_t groupCount = (maxParticles_ + GPUSimulator::kThreadGroupSize - 1) / GPUSimulator::kThreadGroupSize;
	commandList->Dispatch(groupCount, 1, 1);

	// パーティクルバッファをUAVからSRVへ遷移（コンバーター用）
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		);
		commandList->ResourceBarrier(1, &barrier);
	}

	// ========================================
	// フェーズ2: 変換（Particle → ParticleGPU）
	// ========================================

	// レンダリングバッファをSRVからUAVへ遷移（書き込み用）
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			renderBuffer_.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		commandList->ResourceBarrier(1, &barrier);
	}

	// コンバーター用パイプラインとルートシグネチャを設定
	commandList->SetPipelineState(pipeline->GetConverterPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetConverterRootSignature());
	
	// b0: パーティクル定数バッファ
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());

	// b1: カメラ定数バッファ
	if (camera && camera->GetActiveCamera()) {
		D3D12_GPU_VIRTUAL_ADDRESS cameraAddress = camera->GetActiveCamera()->GetConstantBufferAddress();
		if (cameraAddress != 0) {
			commandList->SetComputeRootConstantBufferView(1, cameraAddress);
		}
	}
	
	// t0: パーティクルバッファ（SRV、読み取り用）
	commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(particleSrvIndex_));

	// u0: レンダリングバッファ（UAV、書き込み用）
	commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(renderUavIndex_));

	// コンピュートシェーダーをディスパッチ
	commandList->Dispatch(groupCount, 1, 1);

	// ========================================
	// フェーズ3: 変換後のバリア処理
	// ========================================

	D3D12_RESOURCE_BARRIER barriers[2];
	
	// パーティクルバッファをSRVからUAVへ遷移（次フレームのシミュレーション用）
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
		particleBuffer_.Get(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	// レンダリングバッファをUAVからSRVへ遷移（描画用）
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
		renderBuffer_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);

	commandList->ResourceBarrier(2, barriers);
	
	// 状態追跡を更新
	particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
}

void GPUSimulator::ReadbackParticles(std::vector<Particle>& outParticles)
{
	if (particleCount_ == 0) return;

	// 前フレームのデータを読み出す（1フレーム遅延）
	void* data = nullptr;
	HRESULT hr = particleReadbackBuffer_->Map(0, nullptr, &data);
	if (SUCCEEDED(hr))
	{
		outParticles.resize(particleCount_);
		memcpy(outParticles.data(), data, sizeof(Particle) * particleCount_);
		particleReadbackBuffer_->Unmap(0, nullptr);
	}

	// 次フレーム用にGPU→CPUコピーをスケジューリング
	auto* cmdList = dxCommon_->GetCommandList();

	// パーティクルバッファをCOPY_SOURCE状態へ遷移
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = particleBuffer_.Get();
	barrier.Transition.StateBefore = particleBufferState_;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdList->ResourceBarrier(1, &barrier);

	// リードバックバッファへコピー
	cmdList->CopyResource(particleReadbackBuffer_.Get(), particleBuffer_.Get());

	// パーティクルバッファをUAV状態へ戻す
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	cmdList->ResourceBarrier(1, &barrier);
	
	particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
}
