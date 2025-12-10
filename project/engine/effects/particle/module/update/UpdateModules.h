#pragma once
#include "effects/particle/module/IModule.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"

/**
 * @brief 重力モジュール
 */
class GravityModule : public IModule
{
public:
	GravityModule(const Vector3& gravity = { 0, -9.8f, 0 })
		: gravity_(gravity) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			particle.velocity += gravity_ * context.deltaTime;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleUpdate; }
	const char* GetName() const override { return "Gravity"; }

	void SetGravity(const Vector3& gravity) { gravity_ = gravity; }

private:
	Vector3 gravity_ = { 0, -9.8f, 0 };
};

/**
 * @brief ドラッグモジュール
 */
class DragModule : public IModule
{
public:
	DragModule(float drag = 0.1f) : drag_(drag) {}

	void Execute(ParticleContext& context) override
	{
		float factor = 1.0f - drag_ * context.deltaTime;
		for (auto& particle : *context.particles)
		{
			particle.velocity *= factor;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleUpdate; }
	const char* GetName() const override { return "Drag"; }

	void SetDrag(float drag) { drag_ = drag; }

private:
	float drag_ = 0.1f;
};

/**
 * @brief カラーフェードモジュール
 */
class ColorFadeModule : public IModule
{
public:
	ColorFadeModule(const Vector4& startColor = { 1, 1, 1, 1 }, const Vector4& endColor = { 1, 1, 1, 0 })
		: startColor_(startColor), endColor_(endColor) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			float t = particle.NormalizedAge();
			particle.color.x = startColor_.x + (endColor_.x - startColor_.x) * t;
			particle.color.y = startColor_.y + (endColor_.y - startColor_.y) * t;
			particle.color.z = startColor_.z + (endColor_.z - startColor_.z) * t;
			particle.color.w = startColor_.w + (endColor_.w - startColor_.w) * t;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleUpdate; }
	const char* GetName() const override { return "ColorFade"; }

	void SetColors(const Vector4& start, const Vector4& end)
	{
		startColor_ = start;
		endColor_ = end;
	}

private:
	Vector4 startColor_ = { 1, 1, 1, 1 };
	Vector4 endColor_ = { 1, 1, 1, 0 };
};

/**
 * @brief スケールオーバーライフタイムモジュール
 */
class ScaleOverLifetimeModule : public IModule
{
public:
	ScaleOverLifetimeModule(const Vector3& startScale = { 1, 1, 1 }, const Vector3& endScale = { 0, 0, 0 })
		: startScale_(startScale), endScale_(endScale) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			float t = particle.NormalizedAge();
			particle.scale.x = startScale_.x + (endScale_.x - startScale_.x) * t;
			particle.scale.y = startScale_.y + (endScale_.y - startScale_.y) * t;
			particle.scale.z = startScale_.z + (endScale_.z - startScale_.z) * t;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::ParticleUpdate; }
	const char* GetName() const override { return "ScaleOverLifetime"; }

	void SetScales(const Vector3& start, const Vector3& end)
	{
		startScale_ = start;
		endScale_ = end;
	}

private:
	Vector3 startScale_ = { 1, 1, 1 };
	Vector3 endScale_ = { 0, 0, 0 };
};
