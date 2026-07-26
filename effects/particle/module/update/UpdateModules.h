#pragma once
/**
 * @file UpdateModules.h
 * @brief 基本的なパーティクル更新モジュール
 * 
 * 重力、ドラッグ、カラーフェード、スケール変化などの
 * 標準的なパーティクル更新処理。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/ParticleEasing.h"
#include "math/Vector3.h"

namespace KCE
{
/**
 * @brief 重力モジュール
 */
class GravityModule : public IModule
{
public:
GravityModule(const Vector3& gravity = { 0, -9.8f, 0 })
: minGravity_(gravity), maxGravity_(gravity) {}

void Execute(ParticleContext& context) override
{
for (auto& particle : *context.particles)
{
if (particle.IsAlive())
{
Vector3 g = minGravity_;
if (minGravity_ != maxGravity_)
{
float r = DeterministicRandom(particle.id);
g = MathUtils::Lerp(minGravity_, maxGravity_, r);
}
particle.velocity += g * context.deltaTime;
}
}
}

ModulePhase GetPhase() const override { return ModulePhase::Update; }
const char* GetName() const override { return "Gravity"; }
int32_t GetPriority() const override { return ParticleModulePriority::kGravity; }

void SetGravity(const Vector3& gravity) { minGravity_ = gravity; maxGravity_ = gravity; }
void SetGravityRange(const Vector3& min, const Vector3& max) { minGravity_ = min; maxGravity_ = max; }
Vector3 GetMinGravity() const { return minGravity_; }
Vector3 GetMaxGravity() const { return maxGravity_; }

private:
Vector3 minGravity_ = { 0, -9.8f, 0 };
Vector3 maxGravity_ = { 0, -9.8f, 0 };
};

/**
 * @brief ドラッグモジュール
 */
class DragModule : public IModule
{
public:
DragModule(float drag = 0.1f) : minDrag_(drag), maxDrag_(drag) {}

void Execute(ParticleContext& context) override
{
for (auto& particle : *context.particles)
{
if (particle.IsAlive())
{
float d = minDrag_;
if (minDrag_ != maxDrag_)
{
float r = DeterministicRandom(particle.id);
d = MathUtils::Lerp(minDrag_, maxDrag_, r);
}
float factor = 1.0f - d * context.deltaTime;
particle.velocity *= factor;
}
}
}

ModulePhase GetPhase() const override { return ModulePhase::Update; }
const char* GetName() const override { return "Drag"; }
int32_t GetPriority() const override { return ParticleModulePriority::kDrag; }

void SetDrag(float drag) { minDrag_ = drag; maxDrag_ = drag; }
void SetDragRange(float min, float max) { minDrag_ = min; maxDrag_ = max; }
float GetMinDrag() const { return minDrag_; }
float GetMaxDrag() const { return maxDrag_; }

private:
float minDrag_ = 0.1f;
float maxDrag_ = 0.1f;
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

// イージング適用
particle.color = ApplyEasing<Vector4>(easingType_, effectiveStartColor, endColor_, t);
}
}
}

ModulePhase GetPhase() const override { return ModulePhase::Update; }
const char* GetName() const override { return "ColorFade"; }
int32_t GetPriority() const override { return ParticleModulePriority::kColorFade; }

void SetColors(const Vector4& start, const Vector4& end) { startColor_ = start; endColor_ = end; }
void SetStartColor(const Vector4& c) { startColor_ = c; }
void SetEndColor(const Vector4& c) { endColor_ = c; }
Vector4 GetStartColor() const { return startColor_; }
Vector4 GetEndColor() const { return endColor_; }

void SetUseInitialColor(bool use) { useInitialColor_ = use; }
bool GetUseInitialColor() const { return useInitialColor_; }

void SetEasingType(EasingType type) { easingType_ = type; }
EasingType GetEasingType() const { return easingType_; }

private:
Vector4 startColor_ = { 1, 1, 1, 1 };
Vector4 endColor_ = { 1, 1, 1, 0 };
bool useInitialColor_ = false; // trueの場合、パーティクルの初期カラーからフェード
EasingType easingType_ = EasingType::Linear;
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
particle.scale = ApplyEasing<Vector3>(easingType_, startScale_, endScale_, t);
}
}
}

ModulePhase GetPhase() const override { return ModulePhase::Update; }
const char* GetName() const override { return "ScaleOverLifetime"; }
int32_t GetPriority() const override { return ParticleModulePriority::kScaleOverLifetime; }

void SetScales(const Vector3& start, const Vector3& end) { startScale_ = start; endScale_ = end; }
void SetStartScale(const Vector3& s) { startScale_ = s; }
void SetEndScale(const Vector3& s) { endScale_ = s; }
Vector3 GetStartScale() const { return startScale_; }
Vector3 GetEndScale() const { return endScale_; }

void SetEasingType(EasingType type) { easingType_ = type; }
EasingType GetEasingType() const { return easingType_; }

private:
Vector3 startScale_ = { 1, 1, 1 };
Vector3 endScale_ = { 0, 0, 0 };
EasingType easingType_ = EasingType::Linear;
};
} // namespace KCE
