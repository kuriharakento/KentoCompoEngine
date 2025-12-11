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
			if (particle.IsAlive())
			{
				particle.velocity += gravity_ * context.deltaTime;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Gravity"; }
	int32_t GetPriority() const override { return -50; } // 物理系は早めに

	void SetGravity(const Vector3& gravity) { gravity_ = gravity; }
	Vector3 GetGravity() const { return gravity_; }

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
			if (particle.IsAlive())
			{
				particle.velocity *= factor;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Drag"; }
	int32_t GetPriority() const override { return -40; }

	void SetDrag(float drag) { drag_ = drag; }
	float GetDrag() const { return drag_; }

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
			if (particle.IsAlive())
			{
				float t = particle.NormalizedAge();
				
				// 開始カラーを決定
				Vector4 effectiveStartColor = startColor_;
				if (useInitialColor_)
				{
					// InitialColorModuleで設定された初期カラーを使用
					effectiveStartColor = particle.initialColor;
				}
				
				// 線形補間でフェード
				particle.color.x = effectiveStartColor.x + (endColor_.x - effectiveStartColor.x) * t;
				particle.color.y = effectiveStartColor.y + (endColor_.y - effectiveStartColor.y) * t;
				particle.color.z = effectiveStartColor.z + (endColor_.z - effectiveStartColor.z) * t;
				particle.color.w = effectiveStartColor.w + (endColor_.w - effectiveStartColor.w) * t;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "ColorFade"; }
	int32_t GetPriority() const override { return 50; } // 外観系は後で

	void SetColors(const Vector4& start, const Vector4& end) { startColor_ = start; endColor_ = end; }
	void SetStartColor(const Vector4& c) { startColor_ = c; }
	void SetEndColor(const Vector4& c) { endColor_ = c; }
	Vector4 GetStartColor() const { return startColor_; }
	Vector4 GetEndColor() const { return endColor_; }
	
	void SetUseInitialColor(bool use) { useInitialColor_ = use; }
	bool GetUseInitialColor() const { return useInitialColor_; }

private:
	Vector4 startColor_ = { 1, 1, 1, 1 };
	Vector4 endColor_ = { 1, 1, 1, 0 };
	bool useInitialColor_ = false; // trueの場合、パーティクルの初期カラーからフェード
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
			if (particle.IsAlive())
			{
				float t = particle.NormalizedAge();
				particle.scale.x = startScale_.x + (endScale_.x - startScale_.x) * t;
				particle.scale.y = startScale_.y + (endScale_.y - startScale_.y) * t;
				particle.scale.z = startScale_.z + (endScale_.z - startScale_.z) * t;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "ScaleOverLifetime"; }
	int32_t GetPriority() const override { return 40; }

	void SetScales(const Vector3& start, const Vector3& end) { startScale_ = start; endScale_ = end; }
	void SetStartScale(const Vector3& s) { startScale_ = s; }
	void SetEndScale(const Vector3& s) { endScale_ = s; }
	Vector3 GetStartScale() const { return startScale_; }
	Vector3 GetEndScale() const { return endScale_; }

private:
	Vector3 startScale_ = { 1, 1, 1 };
	Vector3 endScale_ = { 0, 0, 0 };
};
