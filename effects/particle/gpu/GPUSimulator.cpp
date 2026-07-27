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

namespace KCE
{
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
	if (constantBuffer_)
	{
		constantBuffer_->Unmap(0, nullptr);
	}

	// SRV/UAVディスクリプタの解放
	if (srvManager_)
	{
		if (particleSrvIndex_ != SrvManager::kInvalidSrvIndex) srvManager_->Free(particleSrvIndex_);
		if (particleUavIndex_ != SrvManager::kInvalidSrvIndex) srvManager_->Free(particleUavIndex_);
		if (renderSrvIndex_   != SrvManager::kInvalidSrvIndex) srvManager_->Free(renderSrvIndex_);
		if (renderUavIndex_   != SrvManager::kInvalidSrvIndex) srvManager_->Free(renderUavIndex_);
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
	// （パーティクルバッファ、定数バッファ、レンダリングバッファ）
	CreateBuffers();

	initialized_ = true;
}

void GPUSimulator::CreateBuffers()
{
	auto* device = dxCommon_->GetDevice();

	// パーティクルバッファ（UAV対応）
	// GPUシミュレーションで読み書きするメインバッファ
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

	// アップロードバッファ
	// CPU→GPU転送用の中間バッファ（SpawnParticles時に使用）
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

	// リードバックバッファ（ダブルバッファリングでストール防止）
	for (uint32_t i = 0; i < kNumReadbackBuffers; ++i)
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
			IID_PPV_ARGS(&particleReadbackBuffer_[i])
		);
	}

	// 定数バッファ
	// シミュレーションパラメータを格納（deltaTime, 重力, エミッター位置など）
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

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBuffer_)
		);

		// 定数バッファを永続的にマップ
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
	}

	// レンダリング用バッファ（UAV/SRV対応）
	// コンバートシェーダーで変換したレンダリングデータを格納
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
			D3D12_RESOURCE_STATE_COMMON, // 初期状態はコモン（SRVとして使う直前にバリア）
			nullptr,
			IID_PPV_ARGS(&renderBuffer_)
		);
	}

	// ディスクリプタヒープからスロットを確保
	particleSrvIndex_ = srvManager_->Allocate();  // パーティクルバッファSRV
	particleUavIndex_ = srvManager_->Allocate();  // パーティクルバッファUAV
	renderSrvIndex_ = srvManager_->Allocate();    // レンダリングバッファSRV
	renderUavIndex_ = srvManager_->Allocate();    // レンダリングバッファUAV

	// パーティクルバッファ用SRV作成（構造化バッファビュー）
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

	// パーティクルバッファ用UAV作成（書き込み用ビュー）
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

	// レンダリングバッファ用SRV作成（描画時に参照）
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

	// レンダリングバッファ用UAV作成（コンバートシェーダーが書き込む）
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

void GPUSimulator::UploadParticles(const std::vector<Particle>& particles)
{
	if (particles.empty())
	{
		particleCount_ = 0;
		return;
	}

	// アップロード数を制限
	uint32_t copyCount = (std::min)(static_cast<uint32_t>(particles.size()), maxParticles_);

	// アップロードバッファにパーティクルデータを書き込む（先頭から）
	void* data = nullptr;
	particleUploadBuffer_->Map(0, nullptr, &data);
	memcpy(data, particles.data(), sizeof(Particle) * copyCount);
	particleUploadBuffer_->Unmap(0, nullptr);

	auto* cmdList = dxCommon_->GetCommandList();

	// パーティクルバッファをコピー可能状態に遷移
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = particleBuffer_.Get();
	barrier.Transition.StateBefore = particleBufferState_;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdList->ResourceBarrier(1, &barrier);

	// アップロードバッファからGPUバッファへコピー（先頭から）
	cmdList->CopyBufferRegion(
		particleBuffer_.Get(),
		0,
		particleUploadBuffer_.Get(),
		0,
		copyCount * sizeof(Particle)
	);

	// パーティクルバッファをUAV状態に遷移（シミュレーション用）
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	cmdList->ResourceBarrier(1, &barrier);

	// 状態とカウントを更新
	particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	particleCount_ = copyCount;
}

void GPUSimulator::ClearParticles()
{
	particleCount_ = 0;
	readbackFrameIndex_ = 0;
}

void GPUSimulator::UpdateConstantBuffer(float deltaTime, const std::vector<std::unique_ptr<class IModule>>& modules, const Matrix4x4& emitterWorld, uint32_t simulationSpace)
{
	// 定数バッファにシミュレーションパラメータを書き込む
	if (constantData_)
	{
		constantData_->deltaTime = deltaTime;
		constantData_->totalTime = totalTime_;
		constantData_->particleCount = particleCount_;
		constantData_->maxParticles = maxParticles_;
		constantData_->emitterPosition = emitterPosition_;
		constantData_->gravity = gravity_;
		constantData_->isBillboard = isBillboard_ ? 1 : 0;
		constantData_->simulationSpace = simulationSpace;
		constantData_->emitterWorld = emitterWorld;

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
	// 未初期化、またはパーティクルがない場合は処理をスキップ
	if (!initialized_ || particleCount_ == 0) return;

	// 総時間を更新し、定数バッファを更新
	totalTime_ += deltaTime;
	UpdateConstantBuffer(deltaTime, modules, emitterWorld, simulationSpace);

	auto* commandList = dxCommon_->GetCommandList();
	auto* pipeline = GPUParticlePipeline::GetInstance();

	// パイプラインが無効な場合は処理をスキップ
	if (!pipeline->IsValid()) return;

	// フェーズ0: パーティクルバッファをUAV状態に遷移
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

	// フェーズ1: シミュレーション実行（パーティクル更新）
	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = { srvManager_->GetSrvHeap() };
	commandList->SetDescriptorHeaps(1, heaps);

	// パイプラインとルートシグネチャを設定
	commandList->SetPipelineState(pipeline->GetPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetRootSignature());
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(particleUavIndex_));

	// スレッドグループ数を計算してディスパッチ (現在アクティブなパーティクル数基準)
	uint32_t groupCount = (particleCount_ + GPUSimulator::kThreadGroupSize - 1) / GPUSimulator::kThreadGroupSize;
	if (groupCount == 0) groupCount = 1;
	commandList->Dispatch(groupCount, 1, 1);

	// UAVバリアを張り、モジュール実行に繋ぐ
	{
		D3D12_RESOURCE_BARRIER uavBarrier{};
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = nullptr;
		commandList->ResourceBarrier(1, &uavBarrier);
	}

	// フェーズ1.5: 各モジュールのGPUシミュレーション実行
	for (const auto& module : modules)
	{
		if (module->IsGPUSupported())
		{
			module->DispatchGPU(this, commandList, deltaTime);
		}
	}

	// パーティクルバッファをSRV状態に遷移（コンバータが読み取る）
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		);
		commandList->ResourceBarrier(1, &barrier);
	}

	// フェーズ2: レンダリングデータ変換（Particle → ParticleGPU）
	// レンダリングバッファをUAV状態に遷移（コンバータが書き込む）
	if (renderBufferState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			renderBuffer_.Get(),
			renderBufferState_,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		commandList->ResourceBarrier(1, &barrier);
		renderBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	// コンバータパイプラインを設定
	commandList->SetPipelineState(pipeline->GetConverterPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetConverterRootSignature());
	
	// ルートパラメータ0: 定数バッファ（パーティクルパラメータ）
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());

	// ルートパラメータ1: 定数バッファ（カメラ情報）
	if (camera && camera->GetActiveCamera()) {
		D3D12_GPU_VIRTUAL_ADDRESS cameraAddress = camera->GetActiveCamera()->GetConstantBufferAddress();
		if (cameraAddress != 0) {
			commandList->SetComputeRootConstantBufferView(1, cameraAddress);
		}
	}
	
	// ルートパラメータ2: パーティクルバッファSRV（入力）
	commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(particleSrvIndex_));

	// ルートパラメータ3: レンダリングバッファUAV（出力）
	commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(renderUavIndex_));

	// コンバータを実行
	commandList->Dispatch(groupCount, 1, 1);

	// フェーズ3: バリアで各バッファを次のフレーム用の状態に遷移
	D3D12_RESOURCE_BARRIER barriers[2];
	
	// パーティクルバッファをUAV状態に戻す（次フレームのシミュレーション用）
	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
		particleBuffer_.Get(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	);

	// レンダリングバッファをSRV状態に遷移（描画用）
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
		renderBuffer_.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);

	commandList->ResourceBarrier(2, barriers);
	
	// 状態追跡を更新
	particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	renderBufferState_ = D3D12_RESOURCE_STATE_GENERIC_READ;

	// 次フレームのReadback用に、シミュレーション結果をリードバックバッファへコピーしておく（ダブルバッファリングでストール防止）
	if (particleCount_ > 0)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_.Get(),
			particleBufferState_,
			D3D12_RESOURCE_STATE_COPY_SOURCE
		);
		commandList->ResourceBarrier(1, &barrier);

		uint32_t writeIndex = readbackFrameIndex_ % kNumReadbackBuffers;

		// 必要な件数分だけコピー
		commandList->CopyBufferRegion(
			particleReadbackBuffer_[writeIndex].Get(),
			0,
			particleBuffer_.Get(),
			0,
			particleCount_ * sizeof(Particle)
		);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			particleBuffer_.Get(),
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);
		commandList->ResourceBarrier(1, &barrier);

		particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	// 次のフレームに向けてフレームインデックスを進める
	readbackFrameIndex_++;
}

void GPUSimulator::ReadbackParticles(std::vector<Particle>& outParticles)
{
	if (particleCount_ == 0) return;

	// 初回フレーム（まだ一度もGPUでシミュレーションが終わっていない場合）はスキップ
	if (readbackFrameIndex_ == 0) return;

	// 1フレーム前にコピーされたバッファを読み出す（同期ストールが起きない）
	uint32_t readIndex = (readbackFrameIndex_ - 1) % kNumReadbackBuffers;

	void* data = nullptr;
	HRESULT hr = particleReadbackBuffer_[readIndex]->Map(0, nullptr, &data);
	if (SUCCEEDED(hr))
	{
		outParticles.resize(particleCount_);
		memcpy(outParticles.data(), data, sizeof(Particle) * particleCount_);
		particleReadbackBuffer_[readIndex]->Unmap(0, nullptr);
	}
}
} // namespace KCE
