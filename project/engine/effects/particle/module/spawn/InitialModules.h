#pragma once
#include "effects/particle/module/IModule.h"
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
			if (particle.age == 0.0f)
			{
				Vector3 offset = MathUtils::RandomVector3(minOffset_, maxOffset_);
				particle.position = context.emitterPosition + offset;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleSpawn; }
	const char* GetName() const override { return "InitialPosition"; }

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
			if (particle.age == 0.0f)
			{
				particle.velocity = MathUtils::RandomVector3(minVelocity_, maxVelocity_);
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleSpawn; }
	const char* GetName() const override { return "InitialVelocity"; }

	void SetVelocityRange(const Vector3& min, const Vector3& max)
	{
		minVelocity_ = min;
		maxVelocity_ = max;
	}

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
			if (particle.age == 0.0f)
			{
				particle.lifetime = MathUtils::RandomFloat(minLifetime_, maxLifetime_);
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleSpawn; }
	const char* GetName() const override { return "InitialLifetime"; }

	void SetLifetimeRange(float min, float max)
	{
		minLifetime_ = min;
		maxLifetime_ = max;
	}

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
		: color_(color) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f)
			{
				particle.color = color_;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleSpawn; }
	const char* GetName() const override { return "InitialColor"; }

	void SetColor(const Vector4& color) { color_ = color; }

private:
	Vector4 color_ = { 1, 1, 1, 1 };
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
			if (particle.age == 0.0f)
			{
				particle.scale = MathUtils::RandomVector3(minScale_, maxScale_);
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleSpawn; }
	const char* GetName() const override { return "InitialScale"; }

	void SetScaleRange(const Vector3& min, const Vector3& max)
	{
		minScale_ = min;
		maxScale_ = max;
	}

private:
	Vector3 minScale_ = { 1, 1, 1 };
	Vector3 maxScale_ = { 1, 1, 1 };
};
