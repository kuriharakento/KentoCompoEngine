#pragma once
/**
 * @file ForceFieldModules.h
 * @brief フォースフィールドモジュール
 * 
 * アトラクター、ボルテックスなどの
 * 力場ベースのパーティクル制御。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/ParticleTypes.h"
#include "math/Vector3.h"
#include <cmath>
#include <algorithm>
#include <d3d12.h>
#include <wrl/client.h>

/**
 * @brief アトラクターモジュール
 * 
 * 指定した点に向かってパーティクルを引き寄せる。
 */
class AttractorModule : public IModule
{
private:
	struct GPUParams
	{
		Vector3 target;
		float strength;
		float radius;
		uint32_t falloffType;
		uint32_t particleCount;
		float deltaTime;
		float padding[2];
	};

public:
	AttractorModule(const Vector3& target = {}, float strength = 1.0f)
		: target_(target), strength_(strength) {}

	~AttractorModule() override
	{
		if (constantBuffer_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
	}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			Vector3 direction = target_ - particle.position;
			float distance = std::sqrt(
				direction.x * direction.x +
				direction.y * direction.y +
				direction.z * direction.z
			);

			if (distance < 0.001f) continue;

			// 正規化
			direction.x /= distance;
			direction.y /= distance;
			direction.z /= distance;

			// 減衰計算
			float force = CalculateFalloff(distance);

			// 速度に加算
			particle.velocity.x += direction.x * force * strength_ * context.deltaTime;
			particle.velocity.y += direction.y * force * strength_ * context.deltaTime;
			particle.velocity.z += direction.z * force * strength_ * context.deltaTime;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Attractor"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kForceField; }

	bool IsGPUSupported() const override { return true; }
	void DispatchGPU(class GPUSimulator* simulator, struct ID3D12GraphicsCommandList* cmdList, float deltaTime) override;

	void SetTarget(const Vector3& target) { target_ = target; }
	Vector3 GetTarget() const { return target_; }
	void SetStrength(float strength) { strength_ = strength; }
	float GetStrength() const { return strength_; }
	void SetFalloffType(FalloffType type) { falloffType_ = type; }
	FalloffType GetFalloffType() const { return falloffType_; }
	void SetRange(float radius) { radius_ = radius; }
	float GetRange() const { return radius_; }

private:
	float CalculateFalloff(float distance) const
	{
		switch (falloffType_)
		{
		case FalloffType::None:
			return 1.0f;
		case FalloffType::Linear:
			return (radius_ > 0.0f) ? (std::max)(0.0f, 1.0f - distance / radius_) : 1.0f;
		case FalloffType::InverseSquare:
			return 1.0f / (1.0f + distance * distance);
		}
		return 1.0f;
	}

private:
	Vector3 target_ = {};
	float strength_ = 1.0f;
	float radius_ = 10.0f;
	FalloffType falloffType_ = FalloffType::InverseSquare;

	// GPU用定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GPUParams* constantData_ = nullptr;
};

/**
 * @brief 渦巻きモジュール
 * 
 * 指定した軸を中心にパーティクルを回転させる。
 */
class VortexModule : public IModule
{
private:
	struct GPUParams
	{
		Vector3 axis;
		float strength;
		Vector3 center;
		float radius;
		uint32_t falloffType;
		float deltaTime;
		uint32_t particleCount;
		float padding;
	};

public:
	VortexModule(const Vector3& axis = { 0, 1, 0 }, float strength = 1.0f)
		: axis_(axis), strength_(strength)
	{
		NormalizeAxis();
	}

	~VortexModule() override
	{
		if (constantBuffer_)
		{
			constantBuffer_->Unmap(0, nullptr);
		}
	}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			// 中心から粒子へのベクトル
			Vector3 toParticle = particle.position - center_;

			// 外積で接線方向を計算
			Vector3 tangent;
			tangent.x = axis_.y * toParticle.z - axis_.z * toParticle.y;
			tangent.y = axis_.z * toParticle.x - axis_.x * toParticle.z;
			tangent.z = axis_.x * toParticle.y - axis_.y * toParticle.x;

			// 減衰計算
			float distance = std::sqrt(
				toParticle.x * toParticle.x +
				toParticle.y * toParticle.y +
				toParticle.z * toParticle.z
			);
			float force = CalculateFalloff(distance);

			// 速度に加算
			particle.velocity.x += tangent.x * force * strength_ * context.deltaTime;
			particle.velocity.y += tangent.y * force * strength_ * context.deltaTime;
			particle.velocity.z += tangent.z * force * strength_ * context.deltaTime;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Vortex"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kCurlNoise; }

	bool IsGPUSupported() const override { return true; }
	void DispatchGPU(class GPUSimulator* simulator, struct ID3D12GraphicsCommandList* cmdList, float deltaTime) override;

	void SetAxis(const Vector3& axis) { axis_ = axis; NormalizeAxis(); }
	Vector3 GetAxis() const { return axis_; }
	void SetCenter(const Vector3& center) { center_ = center; }
	Vector3 GetCenter() const { return center_; }
	void SetStrength(float strength) { strength_ = strength; }
	float GetStrength() const { return strength_; }
	void SetFalloff(FalloffType type) { falloffType_ = type; }
	void SetRange(float radius) { radius_ = radius; }
	float GetRange() const { return radius_; }

private:
	void NormalizeAxis()
	{
		float len = std::sqrt(axis_.x * axis_.x + axis_.y * axis_.y + axis_.z * axis_.z);
		if (len > 0.001f)
		{
			axis_.x /= len;
			axis_.y /= len;
			axis_.z /= len;
		}
	}

	float CalculateFalloff(float distance) const
	{
		switch (falloffType_)
		{
		case FalloffType::None:
			return 1.0f;
		case FalloffType::Linear:
			return (radius_ > 0.0f) ? (std::max)(0.0f, 1.0f - distance / radius_) : 1.0f;
		case FalloffType::InverseSquare:
			return 1.0f / (1.0f + distance * distance);
		}
		return 1.0f;
	}

private:
	Vector3 axis_ = { 0, 1, 0 };
	Vector3 center_ = {};
	float strength_ = 1.0f;
	float radius_ = 10.0f;
	FalloffType falloffType_ = FalloffType::Linear;

	// GPU用定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GPUParams* constantData_ = nullptr;
};

#include "effects/particle/gpu/GPUSimulator.h"
#include "effects/particle/gpu/GPUParticlePipeline.h"
#include "effects/particle/ParticleManager.h"
#include "base/DirectXCommon.h"

inline void AttractorModule::DispatchGPU(GPUSimulator* simulator, ID3D12GraphicsCommandList* cmdList, float deltaTime)
{
	if (!constantBuffer_)
	{
		auto* pm = ParticleManager::GetInstance();
		constantBuffer_ = pm->GetDxCommon()->CreateBufferResource((sizeof(GPUParams) + 255) & ~255);
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
	}

	if (constantData_)
	{
		constantData_->target = target_;
		constantData_->strength = strength_;
		constantData_->radius = radius_;
		constantData_->falloffType = static_cast<uint32_t>(falloffType_);
		constantData_->particleCount = simulator->GetParticleCount();
		constantData_->deltaTime = deltaTime;
	}

	auto* pipeline = GPUParticlePipeline::GetInstance();
	cmdList->SetPipelineState(pipeline->GetAttractorPipelineState());
	cmdList->SetComputeRootSignature(pipeline->GetAttractorRootSignature());
	cmdList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
	
	auto* pm = ParticleManager::GetInstance();
	cmdList->SetComputeRootDescriptorTable(1, pm->GetSrvManager()->GetGPUDescriptorHandle(simulator->GetParticleUavIndex()));

	uint32_t groupCount = (simulator->GetParticleCount() + GPUSimulator::kThreadGroupSize - 1) / GPUSimulator::kThreadGroupSize;
	if (groupCount == 0) groupCount = 1;
	cmdList->Dispatch(groupCount, 1, 1);
	
	// UAVバリア（同期用）
	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = nullptr;
	cmdList->ResourceBarrier(1, &uavBarrier);
}

inline void VortexModule::DispatchGPU(GPUSimulator* simulator, ID3D12GraphicsCommandList* cmdList, float deltaTime)
{
	if (!constantBuffer_)
	{
		auto* pm = ParticleManager::GetInstance();
		constantBuffer_ = pm->GetDxCommon()->CreateBufferResource((sizeof(GPUParams) + 255) & ~255);
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantData_));
	}

	if (constantData_)
	{
		constantData_->axis = axis_;
		constantData_->strength = strength_;
		constantData_->center = center_;
		constantData_->radius = radius_;
		constantData_->falloffType = static_cast<uint32_t>(falloffType_);
		constantData_->deltaTime = deltaTime;
		constantData_->particleCount = simulator->GetParticleCount();
	}

	auto* pipeline = GPUParticlePipeline::GetInstance();
	cmdList->SetPipelineState(pipeline->GetVortexPipelineState());
	cmdList->SetComputeRootSignature(pipeline->GetVortexRootSignature());
	cmdList->SetComputeRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
	
	auto* pm = ParticleManager::GetInstance();
	cmdList->SetComputeRootDescriptorTable(1, pm->GetSrvManager()->GetGPUDescriptorHandle(simulator->GetParticleUavIndex()));

	uint32_t groupCount = (simulator->GetParticleCount() + GPUSimulator::kThreadGroupSize - 1) / GPUSimulator::kThreadGroupSize;
	if (groupCount == 0) groupCount = 1;
	cmdList->Dispatch(groupCount, 1, 1);
	
	// UAVバリア（同期用）
	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = nullptr;
	cmdList->ResourceBarrier(1, &uavBarrier);
}
