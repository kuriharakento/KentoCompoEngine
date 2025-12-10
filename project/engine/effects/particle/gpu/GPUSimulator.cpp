#include "GPUSimulator.h"
#include "effects/particle/gpu/GPUParticlePipeline.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include <algorithm>
#include <DirectXTex/d3dx12.h>

GPUSimulator::GPUSimulator() = default;

GPUSimulator::~GPUSimulator()
{
	if (constantBuffer_)
	{
		constantBuffer_->Unmap(0, nullptr);
	}
	if (cameraConstantBuffer_)
	{
		cameraConstantBuffer_->Unmap(0, nullptr);
	}
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

	// 共有パイプラインを初期化（既に初期化済みなら何もしない）
	GPUParticlePipeline::GetInstance()->Initialize(dxCommon);

	// このエミッター用のバッファを作成
	CreateBuffers();

	initialized_ = true;
}

void GPUSimulator::CreateBuffers()
{
	auto* device = dxCommon_->GetDevice();

	// パーティクルバッファ（UAV）
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

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&particleBuffer_)
		);
	}

	// アップロードバッファ
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

	// リードバックバッファ
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

	// 定数バッファ
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = (sizeof(GPUParticleConstants) + 255) & ~255;
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

		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
	}

	// カメラバッファ
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = (sizeof(GPUCameraData) + 255) & ~255;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&cameraConstantBuffer_)
		);

		cameraConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
	}

	// レンダリング用バッファ (UAV/SRV)
	{
		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(
			sizeof(ParticleGPU) * maxParticles_,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		);

		dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON, // 初期状態はコモン (SRVとして使う直前にバリア)
			nullptr,
			IID_PPV_ARGS(&renderBuffer_)
		);
	}

	// ディスクリプタの確保
	particleSrvIndex_ = srvManager_->Allocate();
	particleUavIndex_ = srvManager_->Allocate();
	renderSrvIndex_ = srvManager_->Allocate();
	renderUavIndex_ = srvManager_->Allocate();

	// SRV作成
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

	// UAV作成
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

	// Render Buffer SRV
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

	// Render Buffer UAV
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

	void* data = nullptr;
	particleUploadBuffer_->Map(0, nullptr, &data);
	size_t copyCount = (std::min)(newParticles.size(), static_cast<size_t>(maxParticles_ - particleCount_));
	memcpy(static_cast<Particle*>(data) + particleCount_, newParticles.data(), sizeof(Particle) * copyCount);
	particleUploadBuffer_->Unmap(0, nullptr);

	auto* cmdList = dxCommon_->GetCommandList();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = particleBuffer_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdList->ResourceBarrier(1, &barrier);

	cmdList->CopyBufferRegion(
		particleBuffer_.Get(),
		particleCount_ * sizeof(Particle),
		particleUploadBuffer_.Get(),
		particleCount_ * sizeof(Particle),
		copyCount * sizeof(Particle)
	);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	cmdList->ResourceBarrier(1, &barrier);

	particleCount_ += static_cast<uint32_t>(copyCount);
}

void GPUSimulator::UpdateConstantBuffer(float deltaTime)
{
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
	if (!initialized_) return;

	totalTime_ += deltaTime; // Moved from old Dispatch
	UpdateConstantBuffer(deltaTime);

	auto* commandList = dxCommon_->GetCommandList();
	auto* pipeline = GPUParticlePipeline::GetInstance();

	if (!pipeline->IsValid()) return;

	//----------------------------------------
	// 1. Simulation Phase (Particle Buffer Update)
	//----------------------------------------

	commandList->SetPipelineState(pipeline->GetPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetRootSignature());
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(particleUavIndex_));

	uint32_t groupCount = (maxParticles_ + GPUSimulator::kThreadGroupSize - 1) / GPUSimulator::kThreadGroupSize;
	commandList->Dispatch(groupCount, 1, 1);

	// Barrier: ParticleBuffer UAV -> SRV (For Converter)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE // SRV
		);
		commandList->ResourceBarrier(1, &barrier);
	}

	//----------------------------------------
	// 2. Conversion Phase (Particle -> ParticleGPU)
	//----------------------------------------

	// Barrier: RenderBuffer SRV -> UAV (For Writing)
	// 初期状態はCommonだが、前回フレーム描画でSRVになっているはず
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			renderBuffer_.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, // Or COMMON if first time?
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);

		commandList->ResourceBarrier(1, &barrier);
	}

	commandList->SetPipelineState(pipeline->GetConverterPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetConverterRootSignature());
	
	// b0: Constants
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());

	// b1: Camera
	if (camera && camera->GetActiveCamera()) {
		// カメラデータを更新
		Camera* activeCamera = camera->GetActiveCamera();
		if (cameraData_)
		{
			cameraData_->view = activeCamera->GetViewMatrix();
			cameraData_->projection = activeCamera->GetProjectionMatrix();
			cameraData_->eye = activeCamera->GetTranslate();
		}
		commandList->SetComputeRootConstantBufferView(1, cameraConstantBuffer_->GetGPUVirtualAddress());
	}
	
	// t0: Particle Buffer SRV
	commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(particleSrvIndex_));

	// u0: Render Buffer UAV
	commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(renderUavIndex_));

	commandList->Dispatch(groupCount, 1, 1);

	//----------------------------------------
	// 3. Post-Conversion Barriers
	//----------------------------------------

	D3D12_RESOURCE_BARRIER barriers[2];
	
	// ParticleBuffer SRV -> UAV (For next frame Sim)
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
		particleBuffer_.Get(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	// RenderBuffer UAV -> SRV (For Drawing)
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
		renderBuffer_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_GENERIC_READ // used as SRV
	);

	commandList->ResourceBarrier(2, barriers);
}

void GPUSimulator::ReadbackParticles(std::vector<Particle>& outParticles)
{
	if (particleCount_ == 0) return;

	// 1. 前フレームのデータを読み出す（1フレーム遅延）
	// コマンドリストの実行を待たずにMapするため、前回のCopy完了済みのデータを読むことになる
	void* data = nullptr;
	HRESULT hr = particleReadbackBuffer_->Map(0, nullptr, &data);
	if (SUCCEEDED(hr))
	{
		outParticles.resize(particleCount_);
		memcpy(outParticles.data(), data, sizeof(Particle) * particleCount_);
		particleReadbackBuffer_->Unmap(0, nullptr);
	}

	// 2. 次フレームのためのコピーをスケジューリング
	auto* cmdList = dxCommon_->GetCommandList();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = particleBuffer_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdList->ResourceBarrier(1, &barrier);

	cmdList->CopyResource(particleReadbackBuffer_.Get(), particleBuffer_.Get());

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	cmdList->ResourceBarrier(1, &barrier);
}
