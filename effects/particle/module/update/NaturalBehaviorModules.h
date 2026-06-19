#pragma once
/**
 * @file NaturalBehaviorModules.h
 * @brief 自然な挙動（ジッター、速度アライメント等）を制御するモジュール群
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEasing.h"
#include "math/MathUtils.h"
#include <cmath>
#include <numbers>

/**
 * @brief 速度方向へのアライメントモジュール
 * パーティクルの回転を進行方向に合わせる。
 */
class FaceVelocityModule : public IModule
{
public:
	FaceVelocityModule(bool use2D = false) : use2DAlignment_(use2D) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.IsAlive())
			{
				Vector3 velocity = particle.velocity;
				float speedSq = velocity.LengthSquared();
				if (speedSq > 0.0001f)
				{
					if (use2DAlignment_)
					{
						// 2Dスプライト向け：XY平面上の進行方向を Z に設定
						// スプライトが元々「上(+Y)向き」であることを想定し、-PI/2 で調整
						particle.rotation.z = std::atan2(velocity.y, velocity.x) - (std::numbers::pi_v<float> * 0.5f);
					}
					else
					{
						// 3Dメッシュ向け：進行方向を向く Pitch/Yaw を計算 (Z-forward 前提)
						Vector3 rot = MathUtils::CalculateYawPitchFromDirection(velocity);
						particle.rotation.x = rot.x;
						particle.rotation.y = rot.y;
						// 3Dモードでは Z(ロール) は 0 に固定（ねじれ防止）
						particle.rotation.z = 0.0f;
					}
				}
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "FaceVelocity"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kFaceVelocity; }

	void SetUse2DAlignment(bool use) { use2DAlignment_ = use; }
	bool IsUse2DAlignment() const { return use2DAlignment_; }

private:
	bool use2DAlignment_ = false;
};

/**
 * @brief ジッター（揺らぎ）モジュール
 * 毎フレーム、ランダムな位置オフセットを加える。
 */
class JitterModule : public IModule
{
public:
	JitterModule(const Vector3& amount = { 0.1f, 0.1f, 0.1f })
		: amount_(amount) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.IsAlive())
			{
				// 乱数で位置を揺らす
				float rx = MathUtils::RandomFloat(-amount_.x, amount_.x);
				float ry = MathUtils::RandomFloat(-amount_.y, amount_.y);
				float rz = MathUtils::RandomFloat(-amount_.z, amount_.z);
				
				particle.position.x += rx;
				particle.position.y += ry;
				particle.position.z += rz;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Jitter"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kJitter; }

	void SetAmount(const Vector3& amount) { amount_ = amount; }
	Vector3 GetAmount() const { return amount_; }

private:
	Vector3 amount_ = { 0.1f, 0.1f, 0.1f };
};

/**
 * @brief 寿命に応じた外力モジュール
 * パーティクルの寿命（NormalizedAge）に従って強さが変化する力を適用する。
 */
class ForceOverLifetimeModule : public IModule
{
public:
	ForceOverLifetimeModule(const Vector3& direction = { 0, 1, 0 }, float startStrength = 1.0f, float endStrength = 0.0f)
		: direction_(direction), startStrength_(startStrength), endStrength_(endStrength) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.IsAlive())
			{
				float t = particle.NormalizedAge();
				float strength = ApplyEasing<float>(easingType_, startStrength_, endStrength_, t);
				
				particle.velocity += direction_ * strength * context.deltaTime;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "ForceOverLifetime"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kForceOverLifetime; }

	void SetDirection(const Vector3& dir) { direction_ = dir; }
	Vector3 GetDirection() const { return direction_; }

	void SetStrengths(float start, float end) { startStrength_ = start; endStrength_ = end; }
	void SetStartStrength(float s) { startStrength_ = s; }
	void SetEndStrength(float s) { endStrength_ = s; }
	float GetStartStrength() const { return startStrength_; }
	float GetEndStrength() const { return endStrength_; }

	void SetEasingType(EasingType type) { easingType_ = type; }
	EasingType GetEasingType() const { return easingType_; }

private:
	Vector3 direction_ = { 0, 1, 0 };
	float startStrength_ = 1.0f;
	float endStrength_ = 0.0f;
	EasingType easingType_ = EasingType::Linear;
};
