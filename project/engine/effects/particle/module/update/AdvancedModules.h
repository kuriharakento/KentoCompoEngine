#pragma once
/**
 * @file AdvancedModules.h
 * @brief 高度なパーティクルモジュール
 * 
 * 回転、オービット、ノイズ、速度制限などの
 * 高度なパーティクル制御。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <cmath>
#include <numbers>

/**
 * @brief 回転オーバーライフタイムモジュール
 * Z軸まわりの回転をクォータニオンで適用
 */
class RotationOverLifetimeModule : public IModule
{
public:
	RotationOverLifetimeModule(float rotationSpeed = 180.0f)
		: rotationSpeed_(rotationSpeed) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.IsAlive())
			{
				// Z軸まわりの回転（ラジアン）
				float angleRad = rotationSpeed_ * context.deltaTime * (std::numbers::pi_v<float> / 180.0f);
				
				// オイラー角（Z軸）に加算
				particle.rotation.z += angleRad;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "RotationOverLifetime"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kRotationOverLifetime; }

	void SetRotationSpeed(float speed) { rotationSpeed_ = speed; }
	float GetRotationSpeed() const { return rotationSpeed_; }

private:
	float rotationSpeed_ = 180.0f; // degrees per second
};

/**
 * @brief 軌道（オービット）モジュール
 * パーティクルをエミッター中心で周回させる
 */
class OrbitModule : public IModule
{
public:
	OrbitModule(float speed = 90.0f, const Vector3& axis = { 0, 1, 0 })
		: orbitSpeed_(speed), orbitAxis_(axis.Normalize()) {}

	void Execute(ParticleContext& context) override
	{
		float angleRad = orbitSpeed_ * context.deltaTime * (std::numbers::pi_v<float> / 180.0f);
		float cosA = std::cos(angleRad);
		float sinA = std::sin(angleRad);

		for (auto& particle : *context.particles)
		{
			if (particle.IsAlive())
			{
				// エミッターからの相対位置
				Vector3 relPos = particle.position - context.emitterPosition;

				// ロドリゲスの回転公式
				// v' = v*cos(a) + (k x v)*sin(a) + k*(k・v)*(1-cos(a))
				float dotKV = orbitAxis_.x * relPos.x + orbitAxis_.y * relPos.y + orbitAxis_.z * relPos.z;
				
				Vector3 crossKV;
				crossKV.x = orbitAxis_.y * relPos.z - orbitAxis_.z * relPos.y;
				crossKV.y = orbitAxis_.z * relPos.x - orbitAxis_.x * relPos.z;
				crossKV.z = orbitAxis_.x * relPos.y - orbitAxis_.y * relPos.x;

				Vector3 rotated;
				rotated.x = relPos.x * cosA + crossKV.x * sinA + orbitAxis_.x * dotKV * (1 - cosA);
				rotated.y = relPos.y * cosA + crossKV.y * sinA + orbitAxis_.y * dotKV * (1 - cosA);
				rotated.z = relPos.z * cosA + crossKV.z * sinA + orbitAxis_.z * dotKV * (1 - cosA);

				particle.position = context.emitterPosition + rotated;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Orbit"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kForceField; }

	void SetOrbitSpeed(float speed) { orbitSpeed_ = speed; }
	float GetOrbitSpeed() const { return orbitSpeed_; }

	void SetOrbitAxis(const Vector3& axis) { orbitAxis_ = axis.Normalize(); }
	Vector3 GetOrbitAxis() const { return orbitAxis_; }

private:
	float orbitSpeed_ = 90.0f; // degrees per second
	Vector3 orbitAxis_ = { 0, 1, 0 };
};

/**
 * @brief ノイズモジュール
 * パーティクルにランダムな動きを加える
 */
class NoiseModule : public IModule
{
public:
	NoiseModule(float strength = 1.0f, float frequency = 1.0f)
		: strength_(strength), frequency_(frequency) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.IsAlive())
			{
				// シンプルなパーリンノイズ風の動き
				float t = particle.age * frequency_;
				float idOffset = static_cast<float>(particle.id);
				float noiseX = std::sin(t * 2.0f + idOffset * 0.1f) * strength_;
				float noiseY = std::sin(t * 2.3f + idOffset * 0.2f) * strength_;
				float noiseZ = std::sin(t * 2.7f + idOffset * 0.3f) * strength_;

				particle.velocity.x += noiseX * context.deltaTime;
				particle.velocity.y += noiseY * context.deltaTime;
				particle.velocity.z += noiseZ * context.deltaTime;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Noise"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kCurlNoise; }

	void SetStrength(float strength) { strength_ = strength; }
	float GetStrength() const { return strength_; }

	void SetFrequency(float frequency) { frequency_ = frequency; }
	float GetFrequency() const { return frequency_; }

private:
	float strength_ = 1.0f;
	float frequency_ = 1.0f;
};

/**
 * @brief 速度リミットモジュール
 */
class VelocityLimitModule : public IModule
{
public:
	VelocityLimitModule(float maxSpeed = 10.0f) : maxSpeed_(maxSpeed) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.IsAlive())
			{
				float speed = particle.velocity.Length();
				if (speed > maxSpeed_)
				{
					float scale = maxSpeed_ / speed;
					particle.velocity.x *= scale;
					particle.velocity.y *= scale;
					particle.velocity.z *= scale;
				}
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "VelocityLimit"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kVelocityLimit; }

	void SetMaxSpeed(float speed) { maxSpeed_ = speed; }
	float GetMaxSpeed() const { return maxSpeed_; }

private:
	float maxSpeed_ = 10.0f;
};
