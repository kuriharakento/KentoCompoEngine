#include "GPUSimulator.h"
#include "effects/particle/gpu/GPUParticlePipeline.h"
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/module/update/AdvancedModules.h"
#include "effects/particle/module/update/MotionEffectModules.h"
#include "effects/particle/module/update/NaturalBehaviorModules.h"
#include "effects/particle/module/update/TextureSheetModule.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/spawn/SpawnShapeModules.h"
#include "effects/particle/module/ModuleRuntime.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <DirectXTex/d3dx12.h>
#include "effects/particle/ParticleManager.h"
#include "effects/particle/diagnostics/ParticleDiagnostics.h"

namespace KCE
{
namespace
{
void RequireD3D(HRESULT result, const char* stage)
{
	if (FAILED(result)) throw std::runtime_error(std::string(stage) + " failed (HRESULT=" + std::to_string(static_cast<uint32_t>(result)) + ")");
}
	// 定数バッファのアライメント（256バイト境界）
	constexpr size_t kConstantBufferAlignment = 256;
	
	// デフォルト重力（Y軸下向き）
	constexpr float kDefaultGravityY = -9.8f;
	struct GPUEmitterState
	{
		float rateAccumulator = 0.0f;
		float burstElapsed = 0.0f;
		uint32_t burstLoop = 0;
		uint32_t burstFired = 0;
		uint32_t spawnCount = 0;
		uint32_t regularSpawnCount = 0;
		uint32_t eventSpawnCount = 0;
		uint32_t spawnSerialBase = 0;
		uint32_t totalSpawned = 0;
		uint32_t rateSpawnCount = 0;
		uint32_t padding[2]{};
	};
	static_assert(sizeof(GPUEmitterState) == 48);
}

GPUSimulator::GPUSimulator() = default;

GPUSimulator::~GPUSimulator()
{
	// 定数バッファのアンマップ
	if (constantBuffer_)
	{
		constantBuffer_->Unmap(0, nullptr);
	}
	if (moduleProgramBuffer_) moduleProgramBuffer_->Unmap(0, nullptr);
	if (moduleLutBuffer_) moduleLutBuffer_->Unmap(0, nullptr);

	ReleaseDescriptors();
}

void GPUSimulator::ReleaseDescriptors()
{
	uint32_t* descriptors[] = { &particleSrvIndex_, &particleUavIndex_, &renderSrvIndex_, &renderUavIndex_,
		&spawnCounterUavIndex_, &drawArgumentsUavIndex_, &emitterStateUavIndex_, &eventSrvIndex_, &eventUavIndex_,
		&eventCounterSrvIndex_, &eventCounterUavIndex_, &nullEventSrvIndex_, &nullEventCounterSrvIndex_,
		&ribbonPrefixUavIndex_, &ribbonGroupCountUavIndex_, &ribbonGroupOffsetUavIndex_, &ribbonVertexUavIndex_,
		&ribbonDrawArgumentsUavIndex_, &ribbonSortUavIndex_, &moduleProgramSrvIndex_, &moduleLutSrvIndex_ };
	for (uint32_t* descriptor : descriptors)
	{
		if (*descriptor != SrvManager::kInvalidSrvIndex && srvManager_) srvManager_->Free(*descriptor);
		*descriptor = SrvManager::kInvalidSrvIndex;
	}
}

void GPUSimulator::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t maxParticles)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	maxParticles_ = maxParticles;
	initialized_ = false;
	if (!dxCommon_ || !srvManager_ || maxParticles_ == 0)
	{
		Logger::Log("GPUSimulator initialization rejected invalid arguments\n", Logger::LogLevel::Error);
		return;
	}

	// An emitter needs all of its descriptors or none of them. A contiguous
	// transaction prevents a half-initialized simulator when the global heap is
	// exhausted and also makes rollback deterministic.
	constexpr uint32_t kDescriptorCount = 21;
	uint32_t descriptorBase = SrvManager::kInvalidSrvIndex;
	if (!srvManager_->TryAllocateRange(kDescriptorCount, descriptorBase))
	{
		Logger::Log("GPUSimulator initialization failed: descriptor heap exhausted\n", Logger::LogLevel::Error);
		return;
	}
	uint32_t nextDescriptor = descriptorBase;
	particleSrvIndex_ = nextDescriptor++;
	particleUavIndex_ = nextDescriptor++;
	renderSrvIndex_ = nextDescriptor++;
	renderUavIndex_ = nextDescriptor++;
	spawnCounterUavIndex_ = nextDescriptor++;
	drawArgumentsUavIndex_ = nextDescriptor++;
	emitterStateUavIndex_ = nextDescriptor++;
	eventSrvIndex_ = nextDescriptor++;
	eventUavIndex_ = nextDescriptor++;
	eventCounterSrvIndex_ = nextDescriptor++;
	eventCounterUavIndex_ = nextDescriptor++;
	nullEventSrvIndex_ = nextDescriptor++;
	nullEventCounterSrvIndex_ = nextDescriptor++;
	ribbonPrefixUavIndex_ = nextDescriptor++;
	ribbonGroupCountUavIndex_ = nextDescriptor++;
	ribbonGroupOffsetUavIndex_ = nextDescriptor++;
	ribbonVertexUavIndex_ = nextDescriptor++;
	ribbonDrawArgumentsUavIndex_ = nextDescriptor++;
	ribbonSortUavIndex_ = nextDescriptor++;
	moduleProgramSrvIndex_ = nextDescriptor++;
	moduleLutSrvIndex_ = nextDescriptor++;

	try
	{
		GPUParticlePipeline::GetInstance()->Initialize(dxCommon);
		CreateBuffers();
	}
	catch (const std::exception& error)
	{
		Logger::Log(std::string("GPUSimulator initialization failed: ") + error.what() + "\n", Logger::LogLevel::Error);
		ReleaseDescriptors();
		return;
	}

	initialized_ = true;
}

void GPUSimulator::CreateBuffers()
{
	auto* device = dxCommon_->GetDevice();

	// Pure GPU dead-slot allocator. The 4-byte counter is reset by a GPU copy
	// each frame, then atomically incremented by threads claiming dead slots.
	{
		D3D12_HEAP_PROPERTIES defaultHeap{};
		defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = sizeof(uint32_t);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		RequireD3D(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&spawnCounterBuffer_)), "spawn counter buffer");

		D3D12_HEAP_PROPERTIES uploadHeap{};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		RequireD3D(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&spawnCounterResetBuffer_)), "spawn counter reset buffer");
		void* mapped = nullptr;
		RequireD3D(spawnCounterResetBuffer_->Map(0, nullptr, &mapped), "spawn counter reset map");
		*static_cast<uint32_t*>(mapped) = 0;
		spawnCounterResetBuffer_->Unmap(0, nullptr);
	}

	// Sprite indirect draw arguments: VertexCountPerInstance, InstanceCount,
	// StartVertexLocation, StartInstanceLocation.
	{
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = sizeof(D3D12_DRAW_ARGUMENTS);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		D3D12_HEAP_PROPERTIES defaultHeap{};
		defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		RequireD3D(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&drawArgumentsBuffer_)), "draw arguments buffer");

		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_HEAP_PROPERTIES uploadHeap{};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		RequireD3D(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&drawArgumentsResetBuffer_)), "draw arguments reset buffer");
		void* mapped = nullptr;
		RequireD3D(drawArgumentsResetBuffer_->Map(0, nullptr, &mapped), "draw arguments reset map");
		*static_cast<D3D12_DRAW_ARGUMENTS*>(mapped) = { 4, 0, 0, 0 };
		drawArgumentsResetBuffer_->Unmap(0, nullptr);
	}

	{
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = sizeof(GPUEmitterState);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		D3D12_HEAP_PROPERTIES defaultHeap{};
		defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		RequireD3D(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&emitterStateBuffer_)), "emitter state buffer");
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_HEAP_PROPERTIES uploadHeap{};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		RequireD3D(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&emitterStateResetBuffer_)), "emitter state reset buffer");
		void* mapped = nullptr;
		RequireD3D(emitterStateResetBuffer_->Map(0, nullptr, &mapped), "emitter state reset map");
		*static_cast<GPUEmitterState*>(mapped) = {};
		emitterStateResetBuffer_->Unmap(0, nullptr);
	}

	{
		D3D12_HEAP_PROPERTIES defaultHeap{};
		defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC eventDesc = CD3DX12_RESOURCE_DESC::Buffer(
			sizeof(GPUParticleEvent) * maxParticles_, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		RequireD3D(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &eventDesc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&eventBuffer_)), "event buffer");

		D3D12_RESOURCE_DESC counterDesc = CD3DX12_RESOURCE_DESC::Buffer(
			sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		RequireD3D(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &counterDesc,
			D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&eventCounterBuffer_)), "event counter buffer");
		D3D12_HEAP_PROPERTIES uploadHeap{};
		uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
		counterDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		RequireD3D(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &counterDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&eventCounterResetBuffer_)), "event counter reset buffer");
		void* mapped = nullptr;
		RequireD3D(eventCounterResetBuffer_->Map(0, nullptr, &mapped), "event counter reset map");
		*static_cast<uint32_t*>(mapped) = 0;
		eventCounterResetBuffer_->Unmap(0, nullptr);
	}

	// Stable-order GPU ribbon work buffers. A two-level prefix scan compacts the
	// fixed particle pool without a CPU readback and emits a triangle strip VB.
	{
		constexpr uint32_t kRibbonVertexStride = sizeof(float) * 9;
		const uint32_t groupCount = (maxParticles_ + kThreadGroupSize - 1) / kThreadGroupSize;
		ribbonSortCapacity_ = 1;
		while (ribbonSortCapacity_ < maxParticles_) ribbonSortCapacity_ <<= 1;
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		auto createUavBuffer = [&](uint64_t size, Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
		{
			D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			RequireD3D(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource)), "ribbon work buffer");
		};
		createUavBuffer(sizeof(uint32_t) * maxParticles_, ribbonPrefixBuffer_);
		createUavBuffer(sizeof(uint32_t) * groupCount, ribbonGroupCountBuffer_);
		createUavBuffer(sizeof(uint32_t) * groupCount, ribbonGroupOffsetBuffer_);
		createUavBuffer(static_cast<uint64_t>(kRibbonVertexStride) * maxParticles_ * 6u, ribbonVertexBuffer_);
		createUavBuffer(sizeof(D3D12_DRAW_ARGUMENTS), ribbonDrawArgumentsBuffer_);
		createUavBuffer(sizeof(uint32_t) * 4ull * ribbonSortCapacity_, ribbonSortBuffer_);
		ribbonVertexState_ = D3D12_RESOURCE_STATE_COMMON;
		ribbonDrawArgumentsState_ = D3D12_RESOURCE_STATE_COMMON;
	}

	// パーティクルバッファ（UAV対応）
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
		RequireD3D(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&particleBuffer_)
		), "particle buffer");
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

		RequireD3D(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&particleUploadBuffer_)
		), "particle upload buffer");
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

		RequireD3D(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBuffer_)
		), "particle constant buffer");

		// 定数バッファを永続的にマップ
		RequireD3D(constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_)), "particle constant buffer map");
	}
	{
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(16384);
		RequireD3D(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&moduleProgramBuffer_)), "module program buffer");
		RequireD3D(moduleProgramBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&moduleProgramData_)), "module program map");
		std::memset(moduleProgramData_, 0, 16384);
	}
	{
		D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(Vector4) * 1024u);
		RequireD3D(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&moduleLutBuffer_)), "module LUT buffer");
		RequireD3D(moduleLutBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&moduleLutData_)), "module LUT map");
		std::memset(moduleLutData_, 0, sizeof(Vector4) * 1024u);
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
		RequireD3D(dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON, // 初期状態はコモン（SRVとして使う直前にバリア）
			nullptr,
			IID_PPV_ARGS(&renderBuffer_)
		), "particle render buffer");
	}

	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.NumElements = 1;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		device->CreateUnorderedAccessView(spawnCounterBuffer_.Get(), nullptr, &uavDesc,
			srvManager_->GetCPUDescriptorHandle(spawnCounterUavIndex_));
	}
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = maxParticles_;
		srvDesc.Buffer.StructureByteStride = sizeof(GPUParticleEvent);
		device->CreateShaderResourceView(eventBuffer_.Get(), &srvDesc, srvManager_->GetCPUDescriptorHandle(eventSrvIndex_));
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.NumElements = maxParticles_;
		uavDesc.Buffer.StructureByteStride = sizeof(GPUParticleEvent);
		device->CreateUnorderedAccessView(eventBuffer_.Get(), nullptr, &uavDesc, srvManager_->GetCPUDescriptorHandle(eventUavIndex_));

		D3D12_SHADER_RESOURCE_VIEW_DESC counterSrv{};
		counterSrv.Format = DXGI_FORMAT_R32_TYPELESS;
		counterSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		counterSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		counterSrv.Buffer.NumElements = 1;
		counterSrv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		device->CreateShaderResourceView(eventCounterBuffer_.Get(), &counterSrv, srvManager_->GetCPUDescriptorHandle(eventCounterSrvIndex_));
		D3D12_UNORDERED_ACCESS_VIEW_DESC counterUav{};
		counterUav.Format = DXGI_FORMAT_R32_TYPELESS;
		counterUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		counterUav.Buffer.NumElements = 1;
		counterUav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		device->CreateUnorderedAccessView(eventCounterBuffer_.Get(), nullptr, &counterUav, srvManager_->GetCPUDescriptorHandle(eventCounterUavIndex_));
		device->CreateShaderResourceView(nullptr, &srvDesc, srvManager_->GetCPUDescriptorHandle(nullEventSrvIndex_));
		device->CreateShaderResourceView(nullptr, &counterSrv, srvManager_->GetCPUDescriptorHandle(nullEventCounterSrvIndex_));
	}
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.NumElements = 1;
		uavDesc.Buffer.StructureByteStride = sizeof(GPUEmitterState);
		device->CreateUnorderedAccessView(emitterStateBuffer_.Get(), nullptr, &uavDesc,
			srvManager_->GetCPUDescriptorHandle(emitterStateUavIndex_));
	}
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.NumElements = sizeof(D3D12_DRAW_ARGUMENTS) / sizeof(uint32_t);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		device->CreateUnorderedAccessView(drawArgumentsBuffer_.Get(), nullptr, &uavDesc,
			srvManager_->GetCPUDescriptorHandle(drawArgumentsUavIndex_));
	}
	{
		const uint32_t groupCount = (maxParticles_ + kThreadGroupSize - 1) / kThreadGroupSize;
		auto createStructuredUav = [&](ID3D12Resource* resource, uint32_t descriptor, uint32_t elements, uint32_t stride)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			desc.Buffer.NumElements = elements;
			desc.Buffer.StructureByteStride = stride;
			device->CreateUnorderedAccessView(resource, nullptr, &desc, srvManager_->GetCPUDescriptorHandle(descriptor));
		};
		createStructuredUav(ribbonPrefixBuffer_.Get(), ribbonPrefixUavIndex_, maxParticles_, sizeof(uint32_t));
		createStructuredUav(ribbonGroupCountBuffer_.Get(), ribbonGroupCountUavIndex_, groupCount, sizeof(uint32_t));
		createStructuredUav(ribbonGroupOffsetBuffer_.Get(), ribbonGroupOffsetUavIndex_, groupCount, sizeof(uint32_t));
		createStructuredUav(ribbonVertexBuffer_.Get(), ribbonVertexUavIndex_, maxParticles_ * 6u, sizeof(float) * 9);
		createStructuredUav(ribbonSortBuffer_.Get(), ribbonSortUavIndex_, ribbonSortCapacity_, sizeof(uint32_t) * 4u);
		D3D12_UNORDERED_ACCESS_VIEW_DESC argsDesc{};
		argsDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		argsDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		argsDesc.Buffer.NumElements = sizeof(D3D12_DRAW_ARGUMENTS) / sizeof(uint32_t);
		argsDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		device->CreateUnorderedAccessView(ribbonDrawArgumentsBuffer_.Get(), nullptr, &argsDesc,
			srvManager_->GetCPUDescriptorHandle(ribbonDrawArgumentsUavIndex_));
	}
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = 16384 / sizeof(uint32_t);
		desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		device->CreateShaderResourceView(moduleProgramBuffer_.Get(), &desc,
			srvManager_->GetCPUDescriptorHandle(moduleProgramSrvIndex_));
	}
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		desc.Buffer.NumElements = 1024;
		desc.Buffer.StructureByteStride = sizeof(Vector4);
		device->CreateShaderResourceView(moduleLutBuffer_.Get(), &desc,
			srvManager_->GetCPUDescriptorHandle(moduleLutSrvIndex_));
	}

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
	lastDispatchWasPure_ = false;
	if (particles.empty())
	{
		particleCount_ = 0;
		return;
	}

	// アップロード数を制限
	uint32_t copyCount = (std::min)(static_cast<uint32_t>(particles.size()), maxParticles_);

	{
		ParticleScopeTimer timer(ParticleProfileScope::UploadCpuMapMemcpy);
		// アップロードバッファにパーティクルデータを書き込む（先頭から）
		void* data = nullptr;
		if (FAILED(particleUploadBuffer_->Map(0, nullptr, &data)) || !data)
		{
			Logger::Log("GPUSimulator particle upload map failed\n", Logger::LogLevel::Error);
			particleCount_ = 0;
			return;
		}
		memcpy(data, particles.data(), sizeof(Particle) * copyCount);
		particleUploadBuffer_->Unmap(0, nullptr);
	}

	// 転送量を記録
	auto* diag = ParticleDiagnostics::GetInstance();
	diag->AddCpuUploadMemcpyBytes(copyCount * sizeof(Particle));
	diag->AddGpuUploadCopyBytes(copyCount * sizeof(Particle));

	{
		ParticleScopeTimer timer(ParticleProfileScope::UploadCommandRecording);
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

		// 状態を更新
		particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	particleCount_ = copyCount;
}

void GPUSimulator::ClearParticles()
{
	particleCount_ = 0;
	gpuPoolInitialized_ = false;
	pureGpuDispatchEver_ = false;
	lastDispatchWasPure_ = false;
	lastSpawnTime_ = -1000000.0f;
	maxSpawnLifetime_ = 0.0f;
}

void GPUSimulator::SetEventSource(GPUSimulator* source, uint32_t trigger, float probability,
	bool inheritVelocity, float velocityScale, bool inheritColor)
{
	eventSource_ = source;
	eventTrigger_ = trigger;
	eventProbability_ = (std::clamp)(probability, 0.0f, 1.0f);
	eventInheritVelocity_ = inheritVelocity;
	eventVelocityScale_ = velocityScale;
	eventInheritColor_ = inheritColor;
}

void GPUSimulator::ClearEventSource()
{
	eventSource_ = nullptr;
}

bool GPUSimulator::SupportsPureGPU(const std::vector<std::unique_ptr<class IModule>>& modules, RendererType rendererType) const
{
	// The 65,536 limit belongs to the bitonic ribbon sort only. Sprite and
	// Mesh compaction dispatches support the full serialized particle capacity.
	if (rendererType == RendererType::Ribbon && maxParticles_ > kThreadGroupSize * kThreadGroupSize) return false;
	if (modules.size() > 255) return false;
	uint32_t burstModuleCount = 0;
	for (const auto& module : modules)
	{
		const std::string name = module->GetName();
		if (name == "SpawnBurst" && ++burstModuleCount > 1) return false;
		const auto* descriptor = ModuleDescriptorRegistry::GetInstance().Find(name);
		if (!descriptor || !descriptor->pureGpuSupported) return false;
	}
	return true;
}

void GPUSimulator::DispatchPure(float deltaTime, CameraManager* camera,
	const std::vector<std::unique_ptr<class IModule>>& modules,
	const Matrix4x4& emitterWorld, uint32_t simulationSpace, bool emitting, bool resetEmitterState)
{
	if (!initialized_) return;
	if (!gpuPoolInitialized_)
	{
		std::vector<Particle> deadPool(maxParticles_);
		UploadParticles(deadPool); // one-time pool initialization, never a per-frame snapshot upload
		auto* commandList = dxCommon_->GetCommandList();
		if (emitterStateBufferState_ != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				emitterStateBuffer_.Get(), emitterStateBufferState_, D3D12_RESOURCE_STATE_COPY_DEST);
			commandList->ResourceBarrier(1, &barrier);
		}
		commandList->CopyBufferRegion(emitterStateBuffer_.Get(), 0, emitterStateResetBuffer_.Get(), 0, sizeof(GPUEmitterState));
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			emitterStateBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
		emitterStateBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		gpuPoolInitialized_ = true;
	}

	float maxLifetime = 1.0f;
	for (const auto& module : modules)
	{
		if (auto* lifetime = dynamic_cast<InitialLifetimeModule*>(module.get()))
		{
			maxLifetime = (std::max)(0.0f, lifetime->GetMaxLifetime());
		}
	}
	pureGpuDispatch_ = true;
	pureGpuEmitting_ = emitting;
	pureGpuResetEmitterState_ = resetEmitterState;
	pureGpuDispatchEver_ = true;
	lastDispatchWasPure_ = true;
	lastSpawnTime_ = emitting ? totalTime_ + deltaTime : lastSpawnTime_;
	maxSpawnLifetime_ = maxLifetime;
	particleCount_ = maxParticles_; // fixed pool; dead entries convert to transparent instances
	Dispatch(deltaTime, camera, modules, emitterWorld, simulationSpace);
	pureGpuDispatch_ = false;
	pureGpuEmitting_ = false;
	pureGpuResetEmitterState_ = false;
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
		constantData_->gpuRibbonWidth = ribbonWidth_;
		constantData_->gpuRibbonWidthFade = ribbonWidthFade_ ? 1u : 0u;
		constantData_->gpuRibbonAlphaFade = ribbonAlphaFade_ ? 1u : 0u;
		constantData_->gpuRibbonGroupCount = ribbonGroupCount_;
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
		constantData_->hasTextureSheet = 0;
		constantData_->textureSheetColumns = 1;
		constantData_->textureSheetRows = 1;
		constantData_->paddingTextureSheet = 0;
		constantData_->pureGpuEnabled = pureGpuDispatch_ ? 1u : 0u;
		constantData_->spawnCount = 0;
		constantData_->spawnSerialBase = 0;
		constantData_->spawnSeed = 0x4b434550u;
		constantData_->initialVelocityMin = {};
		constantData_->initialVelocityMax = {};
		constantData_->initialLifetimeMin = 1.0f;
		constantData_->initialLifetimeMax = 1.0f;
		constantData_->initialScaleMin = { 1.0f, 1.0f, 1.0f };
		constantData_->initialScaleMax = { 1.0f, 1.0f, 1.0f };
		constantData_->initialColorMin = { 1.0f, 1.0f, 1.0f, 1.0f };
		constantData_->initialColorMax = { 1.0f, 1.0f, 1.0f, 1.0f };
		constantData_->hasSpawnShape = 0;
		constantData_->spawnShapeType = 0;
		constantData_->spawnLocation = 0;
		constantData_->spawnEmitFromSurface = 0;
		constantData_->spawnInnerRadius = 0.0f;
		constantData_->spawnOuterRadius = 1.0f;
		constantData_->spawnInitialSpeed = 0.0f;
		constantData_->spawnArcRadians = 6.283185307f;
		constantData_->spawnBoxSize = { 1.0f, 1.0f, 1.0f };
		constantData_->spawnConeHeight = 2.0f;
		constantData_->spawnLineStart = {};
		constantData_->paddingShape0 = 0.0f;
		constantData_->spawnLineEnd = { 0.0f, 1.0f, 0.0f };
		constantData_->paddingShape1 = 0.0f;
		constantData_->gpuSpawnRate = 0.0f;
		constantData_->gpuBurstCount = 0;
		constantData_->gpuBurstInterval = 0.0f;
		constantData_->gpuBurstDelay = 0.0f;
		constantData_->gpuBurstLoops = 0;
		constantData_->gpuEmitterIsEmitting = pureGpuEmitting_ ? 1u : 0u;
		constantData_->gpuEmitterReset = pureGpuResetEmitterState_ ? 1u : 0u;
		constantData_->paddingEmitterState = 0;
		constantData_->hasGpuEventSource = eventSource_ ? 1u : 0u;
		constantData_->gpuEventTrigger = eventTrigger_;
		constantData_->gpuEventProbability = eventProbability_;
		constantData_->gpuEventInheritVelocity = eventInheritVelocity_ ? 1u : 0u;
		constantData_->gpuEventVelocityScale = eventVelocityScale_;
		constantData_->gpuEventInheritColor = eventInheritColor_ ? 1u : 0u;
		constantData_->gpuSpawnLifetime = 1.0f;
		constantData_->paddingGpuEvent = 0;

		// モジュールを走査してパラメータを抽出
		for (const auto& module : modules)
		{
			if (auto* m = dynamic_cast<SpawnRateModule*>(module.get()))
			{
				constantData_->gpuSpawnRate += (std::max)(0.0f, m->GetRate());
			}
			else if (auto* m = dynamic_cast<SpawnBurstModule*>(module.get()))
			{
				constantData_->gpuBurstCount = m->GetCount();
				constantData_->gpuBurstInterval = m->GetInterval();
				constantData_->gpuBurstDelay = m->GetDelay();
				constantData_->gpuBurstLoops = m->GetLoops();
			}
			else if (module->GetName() == std::string("Drag"))
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
			else if (auto* m = dynamic_cast<TextureSheetModule*>(module.get()))
			{
				constantData_->hasTextureSheet = 1;
				constantData_->textureSheetColumns = (std::max)(1u, m->GetColumns());
				constantData_->textureSheetRows = (std::max)(1u, m->GetRows());
			}
			else if (auto* m = dynamic_cast<InitialVelocityModule*>(module.get()))
			{
				constantData_->initialVelocityMin = m->GetMinVelocity();
				constantData_->initialVelocityMax = m->GetMaxVelocity();
			}
			else if (auto* m = dynamic_cast<InitialLifetimeModule*>(module.get()))
			{
				constantData_->initialLifetimeMin = m->GetMinLifetime();
				constantData_->initialLifetimeMax = m->GetMaxLifetime();
			}
			else if (auto* m = dynamic_cast<InitialScaleModule*>(module.get()))
			{
				constantData_->initialScaleMin = m->GetMinScale();
				constantData_->initialScaleMax = m->GetMaxScale();
			}
			else if (auto* m = dynamic_cast<InitialColorModule*>(module.get()))
			{
				constantData_->initialColorMin = m->GetMinColor();
				constantData_->initialColorMax = m->GetMaxColor();
			}
			else if (auto* m = dynamic_cast<SpawnShapeModule*>(module.get()))
			{
				constantData_->hasSpawnShape = 1;
				constantData_->spawnShapeType = static_cast<uint32_t>(m->GetShapeType());
				constantData_->spawnLocation = static_cast<uint32_t>(m->GetSpawnLocation());
				constantData_->spawnEmitFromSurface = m->GetEmitFromSurface() ? 1u : 0u;
				constantData_->spawnInnerRadius = m->GetInnerRadius();
				constantData_->spawnOuterRadius = m->GetOuterRadius();
				constantData_->spawnInitialSpeed = m->GetInitialSpeed();
				constantData_->spawnArcRadians = m->GetArcAngle() * 0.01745329252f;
				constantData_->spawnBoxSize = m->GetBoxSize();
				constantData_->spawnConeHeight = m->GetConeHeight();
				constantData_->spawnLineStart = m->GetLineStart();
				constantData_->spawnLineEnd = m->GetLineEnd();
			}
		}
		constantData_->gpuSpawnLifetime = (std::max)(0.001f, constantData_->initialLifetimeMax);
		UpdateModuleProgram(modules);
	}
}

void GPUSimulator::UpdateModuleProgram(const std::vector<std::unique_ptr<class IModule>>& modules)
{
	if (!moduleProgramData_ || !moduleLutData_ || !constantData_) return;
	std::memset(moduleProgramData_, 0, 16384);
	std::memset(moduleLutData_, 0, sizeof(Vector4) * 1024u);
	uint32_t count = 0;
	uint32_t lutCursor = 0;
	constexpr uint32_t kRecordSize = 64;
	uint32_t cursor = 16;
	auto writeUInt = [&](uint32_t value) { std::memcpy(moduleProgramData_ + cursor, &value, sizeof(value)); cursor += 4; };
	auto writeFloat = [&](float value) { std::memcpy(moduleProgramData_ + cursor, &value, sizeof(value)); cursor += 4; };
	auto nextRecord = [&]() { cursor = 16 + (++count * kRecordSize); };
	for (const auto& module : modules)
	{
		if (count >= 255 || !module || module->GetPhase() != ModulePhase::Update) continue;
		if (auto* value = dynamic_cast<DragModule*>(module.get()))
		{ writeUInt(1); writeFloat(value->GetMinDrag()); writeFloat(value->GetMaxDrag()); nextRecord(); }
		else if (auto* value = dynamic_cast<VelocityOverLifetimeModule*>(module.get()))
		{ writeUInt(2); writeFloat(value->GetStartMultiplier()); writeFloat(value->GetEndMultiplier()); nextRecord(); }
		else if (auto* value = dynamic_cast<NoiseModule*>(module.get()))
		{ writeUInt(3); writeFloat(value->GetStrength()); writeFloat(value->GetFrequency()); nextRecord(); }
		else if (auto* value = dynamic_cast<ColorFadeModule*>(module.get()))
		{
			const Vector4 start = value->GetStartColor(), end = value->GetEndColor();
			writeUInt(4); writeUInt(value->GetUseInitialColor() ? 1u : 0u); writeUInt(static_cast<uint32_t>(value->GetEasingType()));
			writeFloat(start.x); writeFloat(start.y); writeFloat(start.z); writeFloat(start.w);
			writeFloat(end.x); writeFloat(end.y); writeFloat(end.z); writeFloat(end.w);
			const uint32_t lutOffset = lutCursor;
			const uint32_t lutCount = value->HasGradient() && lutCursor + 32u <= 1024u ? 32u : 0u;
			for (uint32_t i = 0; i < lutCount; ++i) moduleLutData_[lutCursor++] = value->GetGradient().Evaluate(static_cast<float>(i) / static_cast<float>(lutCount - 1u));
			writeUInt(lutCount ? 1u : 0u); writeUInt(lutOffset); writeUInt(lutCount); nextRecord();
		}
		else if (auto* value = dynamic_cast<ScaleOverLifetimeModule*>(module.get()))
		{
			const Vector3 start = value->GetStartScale(), end = value->GetEndScale();
			writeUInt(5); writeUInt(static_cast<uint32_t>(value->GetEasingType()));
			writeFloat(start.x); writeFloat(start.y); writeFloat(start.z); writeFloat(end.x); writeFloat(end.y); writeFloat(end.z);
			const uint32_t lutOffset = lutCursor;
			const uint32_t lutCount = value->HasCurve() && lutCursor + 32u <= 1024u ? 32u : 0u;
			for (uint32_t i = 0; i < lutCount; ++i) moduleLutData_[lutCursor++] = { value->GetCurve().Evaluate(static_cast<float>(i) / static_cast<float>(lutCount - 1u)), 0.0f, 0.0f, 0.0f };
			writeUInt(lutCount ? 1u : 0u); writeUInt(lutOffset); writeUInt(lutCount); nextRecord();
		}
		else if (auto* value = dynamic_cast<StretchByVelocityModule*>(module.get()))
		{ writeUInt(6); writeFloat(value->GetStretchFactor()); writeFloat(value->GetMinStretch()); writeFloat(value->GetMaxStretch()); writeUInt(value->GetPreserveVolume() ? 1u : 0u); nextRecord(); }
		else if (auto* value = dynamic_cast<FaceVelocityModule*>(module.get()))
		{ writeUInt(7); writeUInt(value->IsUse2DAlignment() ? 1u : 0u); nextRecord(); }
		else if (auto* value = dynamic_cast<RotationOverLifetimeModule*>(module.get()))
		{ writeUInt(8); writeFloat(value->GetStartSpeed()); writeFloat(value->GetEndSpeed()); writeUInt(static_cast<uint32_t>(value->GetEasingType())); nextRecord(); }
		else if (auto* value = dynamic_cast<AlphaFadeModule*>(module.get()))
		{ writeUInt(9); writeFloat(value->GetStartAlpha()); writeFloat(value->GetEndAlpha()); writeUInt(value->GetEaseIn() ? 1u : 0u); writeUInt(value->GetEaseOut() ? 1u : 0u); nextRecord(); }
		else if (auto* value = dynamic_cast<FlickerModule*>(module.get()))
		{ writeUInt(10); writeFloat(value->GetFrequency()); writeFloat(value->GetMinAlpha()); writeFloat(value->GetMaxAlpha()); writeUInt(value->GetRandomPhase() ? 1u : 0u); writeUInt(value->GetUseNoise() ? 1u : 0u); nextRecord(); }
	}
	constantData_->hasDrag = constantData_->hasVelocityOL = constantData_->hasNoise = 0;
	constantData_->hasColorFade = constantData_->hasScaleOL = constantData_->hasStretchByVelocity = 0;
	constantData_->hasFaceVelocity = constantData_->hasRotationOL = constantData_->hasAlphaFade = constantData_->hasFlicker = 0;
	std::memcpy(moduleProgramData_, &count, sizeof(count));
	ParticleDiagnostics::GetInstance()->RecordGpuEmitter(pureGpuDispatch_, count, lutCursor, 22u);
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
	if (pureGpuDispatch_)
	{
		commandList->CopyBufferRegion(spawnCounterBuffer_.Get(), 0, spawnCounterResetBuffer_.Get(), 0, sizeof(uint32_t));
	}
	D3D12_RESOURCE_BARRIER counterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		spawnCounterBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commandList->ResourceBarrier(1, &counterBarrier);
	if (eventBufferState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			eventBuffer_.Get(), eventBufferState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
		eventBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	if (eventCounterState_ != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			eventCounterBuffer_.Get(), eventCounterState_, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
	}
	commandList->CopyBufferRegion(eventCounterBuffer_.Get(), 0, eventCounterResetBuffer_.Get(), 0, sizeof(uint32_t));
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			eventCounterBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
		eventCounterState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	// ディスクリプタヒープを設定
	ID3D12DescriptorHeap* heaps[] = { srvManager_->GetSrvHeap() };
	commandList->SetDescriptorHeaps(1, heaps);
	const uint32_t sourceEventSrv = eventSource_ ? eventSource_->GetEventSrvIndex() : nullEventSrvIndex_;
	const uint32_t sourceCounterSrv = eventSource_ ? eventSource_->GetEventCounterSrvIndex() : nullEventCounterSrvIndex_;
	if (pureGpuDispatch_ && pipeline->GetSpawnPreparePipelineState())
	{
		commandList->SetPipelineState(pipeline->GetSpawnPreparePipelineState());
		commandList->SetComputeRootSignature(pipeline->GetRootSignature());
		commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
		commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(particleUavIndex_));
		commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(spawnCounterUavIndex_));
		commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(emitterStateUavIndex_));
		commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(eventUavIndex_));
		commandList->SetComputeRootDescriptorTable(5, srvManager_->GetGPUDescriptorHandle(eventCounterUavIndex_));
		commandList->SetComputeRootDescriptorTable(6, srvManager_->GetGPUDescriptorHandle(sourceEventSrv));
		commandList->SetComputeRootDescriptorTable(7, srvManager_->GetGPUDescriptorHandle(sourceCounterSrv));
		commandList->SetComputeRootDescriptorTable(8, srvManager_->GetGPUDescriptorHandle(moduleProgramSrvIndex_));
		commandList->SetComputeRootDescriptorTable(9, srvManager_->GetGPUDescriptorHandle(moduleLutSrvIndex_));
		commandList->Dispatch(1, 1, 1);
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(emitterStateBuffer_.Get());
		commandList->ResourceBarrier(1, &barrier);
	}

	// パイプラインとルートシグネチャを設定
	commandList->SetPipelineState(pipeline->GetPipelineState());
	commandList->SetComputeRootSignature(pipeline->GetRootSignature());
	commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(particleUavIndex_));
	commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(spawnCounterUavIndex_));
	commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(emitterStateUavIndex_));
	commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(eventUavIndex_));
	commandList->SetComputeRootDescriptorTable(5, srvManager_->GetGPUDescriptorHandle(eventCounterUavIndex_));
	commandList->SetComputeRootDescriptorTable(6, srvManager_->GetGPUDescriptorHandle(sourceEventSrv));
	commandList->SetComputeRootDescriptorTable(7, srvManager_->GetGPUDescriptorHandle(sourceCounterSrv));
	commandList->SetComputeRootDescriptorTable(8, srvManager_->GetGPUDescriptorHandle(moduleProgramSrvIndex_));
	commandList->SetComputeRootDescriptorTable(9, srvManager_->GetGPUDescriptorHandle(moduleLutSrvIndex_));

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
	if (drawArgumentsState_ != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			drawArgumentsBuffer_.Get(), drawArgumentsState_, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->ResourceBarrier(1, &barrier);
	}
	{
		D3D12_RESOURCE_BARRIER barriers[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(eventBuffer_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ),
			CD3DX12_RESOURCE_BARRIER::Transition(eventCounterBuffer_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ)
		};
		commandList->ResourceBarrier(2, barriers);
		eventBufferState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
		eventCounterState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
	}
	commandList->CopyBufferRegion(drawArgumentsBuffer_.Get(), 0, drawArgumentsResetBuffer_.Get(), 0, sizeof(D3D12_DRAW_ARGUMENTS));
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			drawArgumentsBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList->ResourceBarrier(1, &barrier);
		drawArgumentsState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
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
	commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(drawArgumentsUavIndex_));

	// コンバータを実行
	commandList->Dispatch(groupCount, 1, 1);

	if (pureGpuDispatch_ && ribbonEnabled_ && pipeline->GetRibbonComputeRootSignature() && camera && camera->GetActiveCamera())
	{
		D3D12_RESOURCE_BARRIER transitions[2];
		uint32_t transitionCount = 0;
		if (ribbonVertexState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			transitions[transitionCount++] = CD3DX12_RESOURCE_BARRIER::Transition(
				ribbonVertexBuffer_.Get(), ribbonVertexState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			ribbonVertexState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		if (ribbonDrawArgumentsState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			transitions[transitionCount++] = CD3DX12_RESOURCE_BARRIER::Transition(
				ribbonDrawArgumentsBuffer_.Get(), ribbonDrawArgumentsState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			ribbonDrawArgumentsState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}
		if (transitionCount) commandList->ResourceBarrier(transitionCount, transitions);

		commandList->SetComputeRootSignature(pipeline->GetRibbonComputeRootSignature());
		commandList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
		commandList->SetComputeRootConstantBufferView(1, camera->GetActiveCamera()->GetConstantBufferAddress());
		commandList->SetComputeRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(particleSrvIndex_));
		commandList->SetComputeRootDescriptorTable(3, srvManager_->GetGPUDescriptorHandle(ribbonPrefixUavIndex_));
		commandList->SetComputeRootDescriptorTable(4, srvManager_->GetGPUDescriptorHandle(ribbonGroupCountUavIndex_));
		commandList->SetComputeRootDescriptorTable(5, srvManager_->GetGPUDescriptorHandle(ribbonGroupOffsetUavIndex_));
		commandList->SetComputeRootDescriptorTable(6, srvManager_->GetGPUDescriptorHandle(ribbonVertexUavIndex_));
		commandList->SetComputeRootDescriptorTable(7, srvManager_->GetGPUDescriptorHandle(ribbonDrawArgumentsUavIndex_));
		commandList->SetComputeRootDescriptorTable(8, srvManager_->GetGPUDescriptorHandle(ribbonSortUavIndex_));
		commandList->SetPipelineState(pipeline->GetRibbonPrefixPipelineState());
		commandList->Dispatch(groupCount, 1, 1);
		D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
		commandList->ResourceBarrier(1, &uavBarrier);
		commandList->SetPipelineState(pipeline->GetRibbonScanPipelineState());
		commandList->Dispatch(1, 1, 1);
		commandList->ResourceBarrier(1, &uavBarrier);
		const uint32_t sortDispatchCount = (ribbonSortCapacity_ + kThreadGroupSize - 1u) / kThreadGroupSize;
		const uint32_t initializeConstants[3] = { 0u, 0u, ribbonSortCapacity_ };
		commandList->SetComputeRoot32BitConstants(9, 3, initializeConstants, 0);
		commandList->SetPipelineState(pipeline->GetRibbonSortInitializePipelineState());
		commandList->Dispatch(sortDispatchCount, 1, 1);
		commandList->ResourceBarrier(1, &uavBarrier);
		commandList->SetPipelineState(pipeline->GetRibbonSortPipelineState());
		for (uint32_t level = 2u; level <= ribbonSortCapacity_; level <<= 1u)
		{
			for (uint32_t mask = level >> 1u; mask > 0u; mask >>= 1u)
			{
				const uint32_t sortConstants[3] = { level, mask, ribbonSortCapacity_ };
				commandList->SetComputeRoot32BitConstants(9, 3, sortConstants, 0);
				commandList->Dispatch(sortDispatchCount, 1, 1);
				commandList->ResourceBarrier(1, &uavBarrier);
			}
		}
		commandList->SetPipelineState(pipeline->GetRibbonEmitPipelineState());
		commandList->Dispatch(groupCount, 1, 1);
		D3D12_RESOURCE_BARRIER ribbonReady[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition(ribbonVertexBuffer_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
			CD3DX12_RESOURCE_BARRIER::Transition(ribbonDrawArgumentsBuffer_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
		};
		commandList->ResourceBarrier(2, ribbonReady);
		ribbonVertexState_ = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		ribbonDrawArgumentsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	}

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
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			drawArgumentsBuffer_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		commandList->ResourceBarrier(1, &barrier);
		drawArgumentsState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	}
	// 状態追跡を更新
	particleBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	renderBufferState_ = D3D12_RESOURCE_STATE_GENERIC_READ;

	counterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
		spawnCounterBuffer_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &counterBarrier);
}

} // namespace KCE
