#pragma once
/**
 * @file InitialModules.h
 * @brief パーティクル初期値設定モジュール
 * 
 * 生成時の位置、速度、寿命、カラー、スケールなどの
 * 初期パラメータを設定するモジュール群。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/MathUtils.h"

/**
 * @brief 初期位置設定モジュール
 */
class InitialPositionModule : public IModule
{
public:
	InitialPositionModule(const Vector3& min = {}, const Vector3& max = {})
		: minOffset_(min), maxOffset_(max) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f && particle.IsAlive())
			{
				Vector3 offset = MathUtils::RandomVector3(minOffset_, maxOffset_);
				particle.position = context.emitterPosition + offset;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "InitialPosition"; }
	int32_t GetPriority() const override { return 10; } // 位置は早めに設定

	void SetOffsetRange(const Vector3& min, const Vector3& max)
	{
		minOffset_ = min;
		maxOffset_ = max;
	}

private:
	Vector3 minOffset_ = {};
	Vector3 maxOffset_ = {};
};

/**
 * @brief 初期速度設定モジュール
 */
class InitialVelocityModule : public IModule
{
public:
	InitialVelocityModule(const Vector3& min = {}, const Vector3& max = {})
		: minVelocity_(min), maxVelocity_(max) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f && particle.IsAlive())
			{
				particle.velocity = MathUtils::RandomVector3(minVelocity_, maxVelocity_);
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "InitialVelocity"; }
	int32_t GetPriority() const override { return 20; }

	void SetVelocityRange(const Vector3& min, const Vector3& max) { minVelocity_ = min; maxVelocity_ = max; }
	void SetMinVelocity(const Vector3& v) { minVelocity_ = v; }
	void SetMaxVelocity(const Vector3& v) { maxVelocity_ = v; }
	Vector3 GetMinVelocity() const { return minVelocity_; }
	Vector3 GetMaxVelocity() const { return maxVelocity_; }

private:
	Vector3 minVelocity_ = {};
	Vector3 maxVelocity_ = {};
};

/**
 * @brief 初期ライフタイム設定モジュール
 */
class InitialLifetimeModule : public IModule
{
public:
	InitialLifetimeModule(float min = 1.0f, float max = 2.0f)
		: minLifetime_(min), maxLifetime_(max) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f && particle.IsAlive())
			{
				particle.lifetime = MathUtils::RandomFloat(minLifetime_, maxLifetime_);
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "InitialLifetime"; }
	int32_t GetPriority() const override { return 5; } // 寿命は最初に設定

	void SetLifetimeRange(float min, float max) { minLifetime_ = min; maxLifetime_ = max; }
	void SetMinLifetime(float v) { minLifetime_ = v; }
	void SetMaxLifetime(float v) { maxLifetime_ = v; }
	float GetMinLifetime() const { return minLifetime_; }
	float GetMaxLifetime() const { return maxLifetime_; }

private:
	float minLifetime_ = 1.0f;
	float maxLifetime_ = 2.0f;
};

/**
 * @brief 初期カラー設定モジュール
 */
class InitialColorModule : public IModule
{
public:
	InitialColorModule(const Vector4& color = { 1, 1, 1, 1 })
		: minColor_(color), maxColor_(color) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f && particle.IsAlive())
			{
				// min〜maxの間でランダム
				particle.color.x = minColor_.x + (maxColor_.x - minColor_.x) * (rand() / static_cast<float>(RAND_MAX));
				particle.color.y = minColor_.y + (maxColor_.y - minColor_.y) * (rand() / static_cast<float>(RAND_MAX));
				particle.color.z = minColor_.z + (maxColor_.z - minColor_.z) * (rand() / static_cast<float>(RAND_MAX));
				particle.color.w = minColor_.w + (maxColor_.w - minColor_.w) * (rand() / static_cast<float>(RAND_MAX));
				
				// 初期カラーを保存（ColorFadeModule等で使用）
				particle.initialColor = particle.color;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "InitialColor"; }
	int32_t GetPriority() const override { return 30; }

	void SetColor(const Vector4& color) { minColor_ = color; maxColor_ = color; }
	void SetMinColor(const Vector4& c) { minColor_ = c; }
	void SetMaxColor(const Vector4& c) { maxColor_ = c; }
	Vector4 GetMinColor() const { return minColor_; }
	Vector4 GetMaxColor() const { return maxColor_; }

private:
	Vector4 minColor_ = { 1, 1, 1, 1 };
	Vector4 maxColor_ = { 1, 1, 1, 1 };
};

/**
 * @brief 初期スケール設定モジュール
 */
class InitialScaleModule : public IModule
{
public:
	InitialScaleModule(const Vector3& min = { 1, 1, 1 }, const Vector3& max = { 1, 1, 1 })
		: minScale_(min), maxScale_(max) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f && particle.IsAlive())
			{
				particle.scale = MathUtils::RandomVector3(minScale_, maxScale_);
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "InitialScale"; }
	int32_t GetPriority() const override { return 25; }

	void SetScaleRange(const Vector3& min, const Vector3& max) { minScale_ = min; maxScale_ = max; }
	void SetMinScale(const Vector3& s) { minScale_ = s; }
	void SetMaxScale(const Vector3& s) { maxScale_ = s; }
	Vector3 GetMinScale() const { return minScale_; }
	Vector3 GetMaxScale() const { return maxScale_; }

private:
	Vector3 minScale_ = { 1, 1, 1 };
	Vector3 maxScale_ = { 1, 1, 1 };
};
