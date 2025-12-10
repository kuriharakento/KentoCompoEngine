#pragma once
#include "effects/particle/module/IModule.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/ParticleTypes.h"
#include "math/Vector3.h"
#include <cmath>
#include <algorithm>

/**
 * @brief アトラクターモジュール
 * 
 * 指定した点に向かってパーティクルを引き寄せる。
 */
class AttractorModule : public IModule
{
public:
	AttractorModule(const Vector3& target = {}, float strength = 1.0f)
		: target_(target), strength_(strength) {}

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
	int32_t GetPriority() const override { return -30; } // 重力と同じくらい

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
};

/**
 * @brief 渦巻きモジュール
 * 
 * 指定した軸を中心にパーティクルを回転させる。
 */
class VortexModule : public IModule
{
public:
	VortexModule(const Vector3& axis = { 0, 1, 0 }, float strength = 1.0f)
		: axis_(axis), strength_(strength)
	{
		NormalizeAxis();
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
	int32_t GetPriority() const override { return -25; }

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
};
