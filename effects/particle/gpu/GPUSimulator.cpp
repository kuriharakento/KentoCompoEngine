#include "GPUSimulator.h"
#include "effects/particle/gpu/GPUParticlePipeline.h"
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/module/update/AdvancedModules.h"
#include "effects/particle/module/update/MotionEffectModules.h"
#include "effects/particle/module/update/NaturalBehaviorModules.h"
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
}

GPUSimulator::GPUSimulator() = default;

GPUSimulator::~GPUSimulator()
{
	// 定数バッファのアンマップ
	for (int i = 0; i < 2; ++i)
	{
		if (constantBuffer_[i])
		{
			constantBuffer_[i]->Unmap(0, nullptr);
		}
	}

	// SRV/UAVディスクリプタの解放
	if (srvManager_)
	{
		for (int i = 0; i < 2; ++i)
		{
			if (particleSrvIndex_[i] != SrvManager::kInvalidSrvIndex) srvManager_->Free(particleSrvIndex_[i]);
			if (particleUavIndex_[i] != SrvManager::kInvalidSrvIndex) srvManager_->Free(particleUavIndex_[i]);
		}
		if (spawnSrvIndex_        != SrvManager::kInvalidSrvIndex) srvManager_->Free(spawnSrvIndex_);
		if (spawnUavIndex_        != SrvManager::kInvalidSrvIndex) srvManager_->Free(spawnUavIndex_);
		if (counterSrvIndex_      != SrvManager::kInvalidSrvIndex) srvManager_->Free(counterSrvIndex_);
		if (counterUavIndex_      != SrvManager::kInvalidSrvIndex) srvManager_->Free(counterUavIndex_);
		if (indirectArgsUavIndex_ != SrvManager::kInvalidSrvIndex) srvManager_->Free(indirectArgsUavIndex_);
		if (renderSrvIndex_       != SrvManager::kInvalidSrvIndex) srvManager_->Free(renderSrvIndex_);
		if (renderUavIndex_       != SrvManager::kInvalidSrvIndex) srvManager_->Free(renderUavIndex_);
	}
}

void GPUSimulator::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t maxParticles)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	maxParticles_ = maxParticles;

	// 共有GPUパイプラインを初期化（既に初期化済みならスキップ）
	GPUParticlePipeline::GetInstance()->Initialize(dxCommon);

	// このシミュレーター用のバッファを作成
	CreateBuffers();

	initialized_ = true;
}

void GPUSimulator::CreateBuffers()
{
	auto* device = dxCommon_->GetDevice();

	// ダブルバッファ用パーティクルバッファ（UAV対応）
	for (int i = 0; i < 2; ++i)
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
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&particleBuffer_[i])
		);
	}

	// スポン用バッファ（DEFAULT & UPLOAD）
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
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&spawnBuffer_)
		);
	}
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
			IID_PPV_ARGS(&spawnUploadBuffer_)
		);
	}

	// 生存数カウンタ用カウンタバッファ & クリアバッファ
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(uint32_t); // 4 bytes
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
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&counterBuffer_)
		);
	}
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(uint32_t); // 4 bytes
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
			IID_PPV_ARGS(&counterClearBuffer_)
		);

		// 0でクリア用バッファを初期化
		void* data = nullptr;
		counterClearBuffer_->Map(0, nullptr, &data);
		std::memset(data, 0, sizeof(uint32_t));
		counterClearBuffer_->Unmap(0, nullptr);
	}

	// 間接描画用引数バッファ
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = 256; // 余裕を持ったサイズに設定
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
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&indirectArgsBuffer_)
		);

		// UPLOAD用間接描画バッファの作成
		D3D12_HEAP_PROPERTIES uploadHeapProps{};
		uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC uploadDesc = resourceDesc;
		uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&indirectArgsUploadBuffer_)
		);
	}

	// 定数バッファ
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = (sizeof(GPUParticleConstants) + 255) & ~255;  // 256バイト境界にアライン
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		for (int i = 0; i < 2; ++i)
		{
			device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&constantBuffer_[i])
			);

			constantBuffer_[i]->Map(0, nullptr, reinterpret_cast<void**>(&constantData_[i]));
		}
	}

	// レンダリング用バッファ
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
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&renderBuffer_)
		);
	}

	// デバッグ用リードバックカウンタバッファ作成
	{
		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = sizeof(uint32_t); // 4 bytes
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&counterReadbackBuffer_)
		);
	}

	// ディスクリプタヒープからスロットを確保
	for (int i = 0; i < 2; ++i)
	{
		particleSrvIndex_[i] = srvManager_->Allocate();
		particleUavIndex_[i] = srvManager_->Allocate();
	}
	spawnSrvIndex_ = srvManager_->Allocate();
	spawnUavIndex_ = srvManager_->Allocate();
	counterSrvIndex_ = srvManager_->Allocate();
	counterUavIndex_ = srvManager_->Allocate();
	indirectArgsUavIndex_ = srvManager_->Allocate();
	renderSrvIndex_ = srvManager_->Allocate();
	renderUavIndex_ = srvManager_->Allocate();

	// ダブルバッファ用SRV/UAV作成
	for (int i = 0; i < 2; ++i)
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
			particleBuffer_[i].Get(),
			&srvDesc,
			srvManager_->GetCPUDescriptorHandle(particleSrvIndex_[i])
		);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = maxParticles_;
		uavDesc.Buffer.StructureByteStride = sizeof(Particle);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->CreateUnorderedAccessView(
			particleBuffer_[i].Get(),
			nullptr,
			&uavDesc,
			srvManager_->GetCPUDescriptorHandle(particleUavIndex_[i])
		);
	}

	// Spawnバッファ用ビュー作成
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
			spawnBuffer_.Get(),
			&srvDesc,
			srvManager_->GetCPUDescriptorHandle(spawnSrvIndex_)
		);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = maxParticles_;
		uavDesc.Buffer.StructureByteStride = sizeof(Particle);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->CreateUnorderedAccessView(
			spawnBuffer_.Get(),
			nullptr,
			&uavDesc,
			srvManager_->GetCPUDescriptorHandle(spawnUavIndex_)
		);
	}

	// Counterバッファ用ビュー作成
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = 1;
		srvDesc.Buffer.StructureByteStride = sizeof(uint32_t);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		device->CreateShaderResourceView(
			counterBuffer_.Get(),
			&srvDesc,
			srvManager_->GetCPUDescriptorHandle(counterSrvIndex_)
		);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = 1;
		uavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->CreateUnorderedAccessView(
			counterBuffer_.Get(),
			nullptr,
			&uavDesc,
			srvManager_->GetCPUDescriptorHandle(counterUavIndex_)
		);
	}

	// IndirectArgsバッファ用UAV作成
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = 1;
		uavDesc.Buffer.StructureByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		device->CreateUnorderedAccessView(
			indirectArgsBuffer_.Get(),
			nullptr,
			&uavDesc,
			srvManager_->GetCPUDescriptorHandle(indirectArgsUavIndex_)
		);
	}

	// レンダリングバッファ用SRV/UAV作成
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

void GPUSimulator::UploadParticles(const std::vector<Particle>& particles)
{
	if (particles.empty())
	{
		spawnCount_ = 0;
		return;
	}

	// 新規発生数を制限
	uint32_t copyCount = (std::min)(static_cast<uint32_t>(particles.size()), maxParticles_);

	// スポンアップロードバッファに新規発生パーティクルデータを書き込む
	void* data = nullptr;
	spawnUploadBuffer_->Map(0, nullptr, &data);
	memcpy(data, particles.data(), sizeof(Particle) * copyCount);
	spawnUploadBuffer_->Unmap(0, nullptr);

	auto* cmdList = dxCommon_->GetCommandList();

	// スポンバッファをコピー先状態に遷移
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		spawnBuffer_.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_COPY_DEST
	);
	cmdList->ResourceBarrier(1, &barrier);

	// アップロードバッファからスポンバッファへコピー
	cmdList->CopyBufferRegion(
		spawnBuffer_.Get(),
		0,
		spawnUploadBuffer_.Get(),
		0,
		copyCount * sizeof(Particle)
	);

	// スポンバッファをSRV状態（NON_PIXEL_SHADER_RESOURCE）に遷移
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		spawnBuffer_.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	);
	cmdList->ResourceBarrier(1, &barrier);

	spawnCount_ = copyCount;
}

void GPUSimulator::ClearParticles()
{
	particleCount_ = 0;
	spawnCount_ = 0;
	dbIndex_ = 0;
}

void GPUSimulator::UpdateConstantBuffer(uint32_t index, float deltaTime, const std::vector<std::unique_ptr<class IModule>>& modules, const Matrix4x4& emitterWorld, uint32_t simulationSpace)
{
	GPUParticleConstants* constantData_ = this->constantData_[index];
	// 定数バッファにシミュレーションパラメータを書き込む
	if (constantData_)
	{
		constantData_->deltaTime = deltaTime;
		constantData_->totalTime = totalTime_;
		constantData_->particleCount = maxParticles_; // シミュレーション対象は最大数に固定
		constantData_->maxParticles = maxParticles_;
		constantData_->emitterPosition = emitterPosition_;
		constantData_->gravity = gravity_;
		constantData_->isBillboard = isBillboard_ ? 1 : 0;
		constantData_->simulationSpace = simulationSpace;
		constantData_->emitterWorld = emitterWorld;
		constantData_->spawnCount = spawnCount_;

		// デフォルト初期値（モジュールが無効な場合）
		constantData_->hasDrag = 0;
		constantData_->hasColorFade = 0;
		constantData_->hasScaleOL = 0;
		constantData_->dragMin = 0.0f;
		constantData_->dragMax = 0.0f;
		constantData_->paddingDrag = 0.0f;
		constantData_->colorFadeUseInitial = 0;
		constantData_->colorFadeEasing = 0;
		constantData_->paddingCF = 0.0f;
		constantData_->colorFadeStart = { 1.0f, 1.0f, 1.0f, 1.0f };
		constantData_->colorFadeEnd = { 1.0f, 1.0f, 1.0f, 1.0f };
		constantData_->scaleOLEasing = 0;
		constantData_->paddingScaleOL[0] = 0.0f;
		constantData_->paddingScaleOL[1] = 0.0f;
		constantData_->scaleOLStart = { 1.0f, 1.0f, 1.0f };
		constantData_->paddingS1 = 0.0f;
		constantData_->scaleOLEnd = { 1.0f, 1.0f, 1.0f };
		constantData_->paddingS2 = 0.0f;

		constantData_->hasNoise = 0;
		constantData_->noiseStrength = 0.0f;
		constantData_->noiseFrequency = 0.0f;
		constantData_->paddingNoise = 0.0f;
		constantData_->hasRotationOL = 0;
		constantData_->rotOLStartSpeed = 0.0f;
		constantData_->rotOLEndSpeed = 0.0f;
		constantData_->rotOLEasing = 0;
		constantData_->hasAlphaFade = 0;
		constantData_->alphaFadeStart = 0.0f;
		constantData_->alphaFadeEnd = 0.0f;
		constantData_->alphaFadeEaseIn = 0;
		constantData_->alphaFadeEaseOut = 0;
		constantData_->paddingAlpha[0] = 0.0f;
		constantData_->paddingAlpha[1] = 0.0f;
		constantData_->paddingAlpha[2] = 0.0f;

		constantData_->hasVelocityOL = 0;
		constantData_->velocityOLStart = 1.0f;
		constantData_->velocityOLEnd = 1.0f;
		constantData_->paddingVelocityOL = 0.0f;

		constantData_->hasStretchByVelocity = 0;
		constantData_->stretchFactor = 0.0f;
		constantData_->minStretch = 1.0f;
		constantData_->maxStretch = 1.0f;
		constantData_->stretchPreserveVolume = 0;
		constantData_->paddingStretch[0] = 0.0f;
		constantData_->paddingStretch[1] = 0.0f;
		constantData_->paddingStretch[2] = 0.0f;

		constantData_->hasFlicker = 0;
		constantData_->flickerFrequency = 0.0f;
		constantData_->flickerMinAlpha = 0.0f;
		constantData_->flickerMaxAlpha = 0.0f;
		constantData_->flickerRandomPhase = 0;
		constantData_->flickerUseNoise = 0;
		constantData_->paddingFlicker[0] = 0.0f;
		constantData_->paddingFlicker[1] = 0.0f;

		constantData_->hasFaceVelocity = 0;
		constantData_->faceVelocityUse2D = 0;
		constantData_->paddingFaceVelocity[0] = 0.0f;
		constantData_->paddingFaceVelocity[1] = 0.0f;

		// モジュールを走査してパラメータを抽出
		for (const auto& module : modules)
		{
			if (module->GetName() == std::string("Drag"))
			{
				auto* m = dynamic_cast<DragModule*>(module.get());
				if (m)
				{
					constantData_->hasDrag = 1;
					constantData_->dragMin = m->GetMinDrag();
					constantData_->dragMax = m->GetMaxDrag();
				}
			}
			else if (module->GetName() == std::string("ColorFade"))
			{
				auto* m = dynamic_cast<ColorFadeModule*>(module.get());
				if (m)
				{
					constantData_->hasColorFade = 1;
					constantData_->colorFadeUseInitial = m->GetUseInitialColor() ? 1 : 0;
					constantData_->colorFadeEasing = static_cast<uint32_t>(m->GetEasingType());
					constantData_->colorFadeStart = m->GetStartColor();
					constantData_->colorFadeEnd = m->GetEndColor();
				}
			}
			else if (module->GetName() == std::string("ScaleOverLifetime"))
			{
				auto* m = dynamic_cast<ScaleOverLifetimeModule*>(module.get());
				if (m)
				{
					constantData_->hasScaleOL = 1;
					constantData_->scaleOLEasing = static_cast<uint32_t>(m->GetEasingType());
					constantData_->scaleOLStart = m->GetStartScale();
					constantData_->scaleOLEnd = m->GetEndScale();
				}
			}
			else if (module->GetName() == std::string("Noise"))
			{
				auto* m = dynamic_cast<NoiseModule*>(module.get());
				if (m)
				{
					constantData_->hasNoise = 1;
					constantData_->noiseStrength = m->GetStrength();
					constantData_->noiseFrequency = m->GetFrequency();
				}
			}
			else if (module->GetName() == std::string("RotationOverLifetime"))
			{
				auto* m = dynamic_cast<RotationOverLifetimeModule*>(module.get());
				if (m)
				{
					constantData_->hasRotationOL = 1;
					constantData_->rotOLStartSpeed = m->GetStartSpeed();
					constantData_->rotOLEndSpeed = m->GetEndSpeed();
					constantData_->rotOLEasing = static_cast<uint32_t>(m->GetEasingType());
				}
			}
			else if (module->GetName() == std::string("AlphaFade"))
			{
				auto* m = dynamic_cast<AlphaFadeModule*>(module.get());
				if (m)
				{
					constantData_->hasAlphaFade = 1;
					constantData_->alphaFadeStart = m->GetStartAlpha();
					constantData_->alphaFadeEnd = m->GetEndAlpha();
					constantData_->alphaFadeEaseIn = m->GetEaseIn() ? 1 : 0;
					constantData_->alphaFadeEaseOut = m->GetEaseOut() ? 1 : 0;
				}
			}
			else if (module->GetName() == std::string("VelocityOverLifetime"))
			{
				auto* m = dynamic_cast<VelocityOverLifetimeModule*>(module.get());
				if (m)
				{
					constantData_->hasVelocityOL = 1;
					constantData_->velocityOLStart = m->GetStartMultiplier();
					constantData_->velocityOLEnd = m->GetEndMultiplier();
				}
			}
			else if (module->GetName() == std::string("StretchByVelocity"))
			{
				auto* m = dynamic_cast<StretchByVelocityModule*>(module.get());
				if (m)
				{
					constantData_->hasStretchByVelocity = 1;
					constantData_->stretchFactor = m->GetStretchFactor();
					constantData_->minStretch = m->GetMinStretch();
					constantData_->maxStretch = m->GetMaxStretch();
					constantData_->stretchPreserveVolume = m->GetPreserveVolume() ? 1 : 0;
				}
			}
			else if (module->GetName() == std::string("Flicker"))
			{
				auto* m = dynamic_cast<FlickerModule*>(module.get());
				if (m)
				{
					constantData_->hasFlicker = 1;
					constantData_->flickerFrequency = m->GetFrequency();
					constantData_->flickerMinAlpha = m->GetMinAlpha();
					constantData_->flickerMaxAlpha = m->GetMaxAlpha();
					constantData_->flickerRandomPhase = m->GetRandomPhase() ? 1 : 0;
					constantData_->flickerUseNoise = m->GetUseNoise() ? 1 : 0;
				}
			}
			else if (module->GetName() == std::string("FaceVelocity"))
			{
				auto* m = dynamic_cast<FaceVelocityModule*>(module.get());
				if (m)
				{
					constantData_->hasFaceVelocity = 1;
					constantData_->faceVelocityUse2D = m->IsUse2DAlignment() ? 1 : 0;
				}
			}
		}
	}
}

void GPUSimulator::Dispatch(float deltaTime, CameraManager* camera, const std::vector<std::unique_ptr<class IModule>>& modules, const Matrix4x4& emitterWorld, uint32_t simulationSpace)
{
	if (!initialized_) return;

	// 前回のリードバックリクエストがスケジュールされ、GPUコピーが完了している場合は値を読み取る
	if (readbackRequested_)
	{
		uint32_t* pData = nullptr;
		if (SUCCEEDED(counterReadbackBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&pData))))
		{
			lastActiveCount_ = *pData;
			counterReadbackBuffer_->Unmap(0, nullptr);
		}
		readbackRequested_ = false;
	}

	// 総時間を更新し、定数バッファを更新
	totalTime_ += deltaTime;
	UpdateConstantBuffer(dbIndex_, deltaTime, modules, emitterWorld, simulationSpace);

	auto* commandList = dxCommon_->GetCommandList();
	auto* pipeline = GPUParticlePipeline::GetInstance();

	if (!pipeline->IsValid()) return;

	// 1. カウンタのクリア
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			counterBuffer_.Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_COPY_DEST
		);
		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyBufferRegion(
			counterBuffer_.Get(),
			0,
			counterClearBuffer_.Get(),
			0,
			sizeof(uint32_t)
		);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			counterBuffer_.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		commandList->ResourceBarrier(1, &barrier);
	}

	// 2. シミュレーション用パーティクルバッファをUAV状態に遷移
	if (particleBufferState_[dbIndex_] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_[dbIndex_].Get(),
			particleBufferState_[dbIndex_],
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		commandList->ResourceBarrier(1, &barrier);
		particleBufferState_[dbIndex_] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	// 3. シミュレーション実行
	ID3D12DescriptorHeap* heaps[] = { srvManager_->GetSrvHeap() };
	commandList->SetDescriptorHeaps(1, heaps);

	commandList->SetPipelineState(pipeline->GetPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetRootSignature());
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_[dbIndex_]->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(particleUavIndex_[dbIndex_]));

	// スレッドグループ数を計算してディスパッチ (最大数で回してFLAG_ALIVEで判別)
	uint32_t groupCount = (maxParticles_ + GPUSimulator::kThreadGroupSize - 1) / GPUSimulator::kThreadGroupSize;
	if (groupCount == 0) groupCount = 1;
	commandList->Dispatch(groupCount, 1, 1);

	// UAVバリアを張り、モジュール実行に繋ぐ
	{
		D3D12_RESOURCE_BARRIER uavBarrier{};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = nullptr;
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	// 4. 各モジュールのGPUシミュレーション実行
	for (const auto& module : modules)
	{
		if (module->IsGPUSupported())
		{
			module->DispatchGPU(this, commandList, deltaTime);
		}
	}

	// 5. 状態遷移: 入力パーティクルバッファをSRVに、出力パーティクルバッファをUAVに
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	
	// 入力パーティクルバッファをSRV遷移
	if (particleBufferState_[dbIndex_] != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_[dbIndex_].Get(),
			particleBufferState_[dbIndex_],
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		));
		particleBufferState_[dbIndex_] = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}

	// 出力パーティクルバッファをUAV遷移
	uint32_t nextDbIndex = 1 - dbIndex_;
	if (particleBufferState_[nextDbIndex] != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_[nextDbIndex].Get(),
			particleBufferState_[nextDbIndex],
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		));
		particleBufferState_[nextDbIndex] = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	// レンダリングバッファをUAV遷移
	if (renderBufferState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			renderBuffer_.Get(),
			renderBufferState_,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		));
		renderBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	if (!barriers.empty())
	{
		commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
	}

	// 6. コンバータパイプラインを実行して生存＋新規発生を詰め直す
	commandList->SetPipelineState(pipeline->GetConverterPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetConverterRootSignature());
	
	// パラメータ設定 (7つ)
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_[dbIndex_]->GetGPUVirtualAddress());

	D3D12_GPU_VIRTUAL_ADDRESS cameraAddress = 0;
	if (camera && camera->GetActiveCamera()) {
		cameraAddress = camera->GetActiveCamera()->GetConstantBufferAddress();
	}
	commandList->SetComputeRootConstantBufferView(1, cameraAddress ? cameraAddress : constantBuffer_[dbIndex_]->GetGPUVirtualAddress()); // ダミー対策
	
	commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(particleSrvIndex_[dbIndex_]));     // gParticles (t0)
	commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(spawnSrvIndex_));                 // gSpawnParticles (t1)
	commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(renderUavIndex_));                // gRenderParticles (u0)
	commandList->SetComputeRootDescriptorTable(5, srvManager_->GetGPUDescriptorHandle(particleUavIndex_[nextDbIndex])); // gOutParticles (u1)
	commandList->SetComputeRootDescriptorTable(6, srvManager_->GetGPUDescriptorHandle(counterUavIndex_));               // gCounter (u2)

	// コンバータを実行 (入力の合計数)
	uint32_t totalInput = maxParticles_ + spawnCount_;
	uint32_t convertGroupCount = (totalInput + GPUSimulator::kThreadGroupSize - 1) / GPUSimulator::kThreadGroupSize;
	if (convertGroupCount == 0) convertGroupCount = 1;
	commandList->Dispatch(convertGroupCount, 1, 1);

	// UAVバリア
	{
		D3D12_RESOURCE_BARRIER uavBarrier{};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = nullptr;
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	// 7. WriteIndirectArgs CSを実行
	D3D12_RESOURCE_BARRIER barriersIndirect[2];
	barriersIndirect[0] = CD3DX12_RESOURCE_BARRIER::Transition(
		counterBuffer_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	);
	barriersIndirect[1] = CD3DX12_RESOURCE_BARRIER::Transition(
		indirectArgsBuffer_.Get(),
		indirectArgsBufferState_,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);
	commandList->ResourceBarrier(2, barriersIndirect);
	indirectArgsBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	commandList->SetPipelineState(pipeline->GetWriteIndirectArgsPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetWriteIndirectArgsRootSignature());
	commandList->SetComputeRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(counterSrvIndex_));
	commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(indirectArgsUavIndex_));

	commandList->Dispatch(1, 1, 1);

	// 8. 最終バリア遷移
	std::vector<D3D12_RESOURCE_BARRIER> barriersFinal;

	// 間接描画バッファを間接引数状態へ
	barriersFinal.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		indirectArgsBuffer_.Get(),
		indirectArgsBufferState_,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
	));
	indirectArgsBufferState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

	// レンダリングバッファをSRV（GENERIC_READ）状態へ
	if (renderBufferState_ != D3D12_RESOURCE_STATE_GENERIC_READ)
	{
		barriersFinal.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			renderBuffer_.Get(),
			renderBufferState_,
			D3D12_RESOURCE_STATE_GENERIC_READ
		));
		renderBufferState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	// カウンタバッファをCOMMONへ戻す
	barriersFinal.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		counterBuffer_.Get(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COMMON
	));

	// スポンバッファをCOMMONへ戻す
	barriersFinal.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
		spawnBuffer_.Get(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COMMON
	));

	if (!barriersFinal.empty())
	{
		commandList->ResourceBarrier(static_cast<UINT>(barriersFinal.size()), barriersFinal.data());
	}

	// ダブルバッファのフリップ
	dbIndex_ = nextDbIndex;

	// CPU側の生存数は最大値に設定 (常にシミュレーションCSを実行するため。早期スキップ判定はEmitter側で行う)
	particleCount_ = maxParticles_;
	spawnCount_ = 0; // スポン完了につきクリア

	// 低頻度リードバックのスケジュール
	frameCounter_++;
	if (frameCounter_ % 30 == 0)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			counterBuffer_.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		);
		commandList->ResourceBarrier(1, &barrier);

		commandList->CopyBufferRegion(
			counterReadbackBuffer_.Get(),
			0,
			counterBuffer_.Get(),
			0,
			sizeof(uint32_t)
		);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			counterBuffer_.Get(),
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		commandList->ResourceBarrier(1, &barrier);

		readbackRequested_ = true;
	}
}

void GPUSimulator::ReadbackParticles(std::vector<Particle>& outParticles)
{
	// 読み戻しは完全に撤廃するため何もしない
	(void)outParticles;
}

void GPUSimulator::InitializeIndirectArgs(RendererType type, uint32_t elementCount)
{
	if (!indirectArgsBuffer_ || !indirectArgsUploadBuffer_) return;

	void* data = nullptr;
	if (SUCCEEDED(indirectArgsUploadBuffer_->Map(0, nullptr, &data)))
	{
		if (type == RendererType::Mesh)
		{
			D3D12_DRAW_INDEXED_ARGUMENTS args{};
			args.IndexCountPerInstance = elementCount;
			args.InstanceCount = 0; // CSが書き換える
			args.StartIndexLocation = 0;
			args.BaseVertexLocation = 0;
			args.StartInstanceLocation = 0;
			memcpy(data, &args, sizeof(args));
		}
		else
		{
			D3D12_DRAW_ARGUMENTS args{};
			args.VertexCountPerInstance = elementCount; // 通常 4
			args.InstanceCount = 0; // CSが書き換える
			args.StartVertexLocation = 0;
			args.StartInstanceLocation = 0;
			memcpy(data, &args, sizeof(args));
		}
		indirectArgsUploadBuffer_->Unmap(0, nullptr);
	}

	auto* commandList = dxCommon_->GetCommandList();
	
	// コピー先状態へ遷移
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		indirectArgsBuffer_.Get(),
		indirectArgsBufferState_,
		D3D12_RESOURCE_STATE_COPY_DEST
	);
	commandList->ResourceBarrier(1, &barrier);
	indirectArgsBufferState_ = D3D12_RESOURCE_STATE_COPY_DEST;

	// アップロードバッファからデフォルトバッファへコピー
	commandList->CopyBufferRegion(
		indirectArgsBuffer_.Get(),
		0,
		indirectArgsUploadBuffer_.Get(),
		0,
		256
	);

	// COMMON状態へ戻す
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		indirectArgsBuffer_.Get(),
		indirectArgsBufferState_,
		D3D12_RESOURCE_STATE_COMMON
	);
	commandList->ResourceBarrier(1, &barrier);
	indirectArgsBufferState_ = D3D12_RESOURCE_STATE_COMMON;
}
