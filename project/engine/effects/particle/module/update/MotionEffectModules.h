#pragma once
/**
 * @file MotionEffectModules.h
 * @brief モーションエフェクト用パーティクルモジュール
 * 
 * VelocityOverLifetime, StretchByVelocity, Wind, Flicker,
 * AlphaFade, RotationBySpeed, SineWave, Spiral, Twist
 * などのモーション・動き関連モジュール群。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/MathUtils.h"
#include <cmath>
#include <numbers>
#include <algorithm>

/**
 * @brief 放射状初期速度モジュール
 * エミッター中心から放射状に初期速度を付与
 */
class RadialVelocityModule : public IModule
{
public:
	RadialVelocityModule(float speed = 5.0f) : speed_(speed) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f && particle.IsAlive())
			{
				// エミッター位置からの方向を計算
				Vector3 direction = particle.position - context.emitterPosition;
				float length = direction.Length();
				
				if (length > 0.001f)
				{
					// 正規化して速度を設定
					direction = direction * (1.0f / length);
				}
				else
				{
					// 位置がエミッターと同じ場合はランダムな方向
					float theta = MathUtils::RandomFloat(0, 2.0f * std::numbers::pi_v<float>);
					float phi = MathUtils::RandomFloat(0, std::numbers::pi_v<float>);
					direction.x = std::sin(phi) * std::cos(theta);
					direction.y = std::cos(phi);
					direction.z = std::sin(phi) * std::sin(theta);
				}
				
				float actualSpeed = MathUtils::RandomFloat(minSpeed_, maxSpeed_);
				particle.velocity = direction * actualSpeed;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "RadialVelocity"; }
	int32_t GetPriority() const override { return 22; } // InitialVelocityより後

	void SetSpeed(float speed) { speed_ = speed; minSpeed_ = speed; maxSpeed_ = speed; }
	float GetSpeed() const { return speed_; }
	void SetSpeedRange(float min, float max) { minSpeed_ = min; maxSpeed_ = max; speed_ = (min + max) * 0.5f; }
	float GetMinSpeed() const { return minSpeed_; }
	float GetMaxSpeed() const { return maxSpeed_; }

private:
	float speed_ = 5.0f;
	float minSpeed_ = 5.0f;
	float maxSpeed_ = 5.0f;
};

/**
 * @brief 速度オーバーライフタイムモジュール
 * 寿命に応じて速度に乗算係数を適用
 */
class VelocityOverLifetimeModule : public IModule
{
public:
	VelocityOverLifetimeModule(float startMultiplier = 1.0f, float endMultiplier = 0.0f)
		: startMultiplier_(startMultiplier), endMultiplier_(endMultiplier) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float t = particle.NormalizedAge();
			float multiplier = startMultiplier_ + (endMultiplier_ - startMultiplier_) * t;
			
			// 速度に乗算（デルタタイム分の補正）
			// 毎フレーム適用されるので、累積的な減速にする
			float dampFactor = 1.0f - (1.0f - multiplier) * context.deltaTime;
			particle.velocity *= dampFactor;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "VelocityOverLifetime"; }
	int32_t GetPriority() const override { return -45; }

	void SetStartMultiplier(float m) { startMultiplier_ = m; }
	float GetStartMultiplier() const { return startMultiplier_; }
	void SetEndMultiplier(float m) { endMultiplier_ = m; }
	float GetEndMultiplier() const { return endMultiplier_; }

private:
	float startMultiplier_ = 1.0f;
	float endMultiplier_ = 0.0f;
};

/**
 * @brief 速度によるストレッチモジュール
 * 速度方向にパーティクルを伸ばす（弾丸、雨などに最適）
 */
class StretchByVelocityModule : public IModule
{
public:
	StretchByVelocityModule(float stretchFactor = 0.1f, float minStretch = 1.0f, float maxStretch = 5.0f)
		: stretchFactor_(stretchFactor), minStretch_(minStretch), maxStretch_(maxStretch) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float speed = particle.velocity.Length();
			float stretch = 1.0f + speed * stretchFactor_;
			stretch = std::clamp(stretch, minStretch_, maxStretch_);

			// Y軸をストレッチ（ビルボードの場合、速度方向に伸びる）
			particle.scale.y = stretch;
			
			// X/Zを縮小して細く見せる（オプション）
			if (preserveVolume_)
			{
				float shrink = 1.0f / std::sqrt(stretch);
				particle.scale.x = shrink;
				particle.scale.z = shrink;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "StretchByVelocity"; }
	int32_t GetPriority() const override { return 44; }

	void SetStretchFactor(float f) { stretchFactor_ = f; }
	float GetStretchFactor() const { return stretchFactor_; }
	void SetMinStretch(float m) { minStretch_ = m; }
	float GetMinStretch() const { return minStretch_; }
	void SetMaxStretch(float m) { maxStretch_ = m; }
	float GetMaxStretch() const { return maxStretch_; }
	void SetPreserveVolume(bool p) { preserveVolume_ = p; }
	bool GetPreserveVolume() const { return preserveVolume_; }

private:
	float stretchFactor_ = 0.1f;
	float minStretch_ = 1.0f;
	float maxStretch_ = 5.0f;
	bool preserveVolume_ = false;
};

/**
 * @brief 風モジュール
 * 方向性のある風の力を適用
 */
class WindModule : public IModule
{
public:
	WindModule(const Vector3& direction = { 1, 0, 0 }, float strength = 1.0f)
		: direction_(direction.Normalize()), strength_(strength) {}

	void Execute(ParticleContext& context) override
	{
		// 時間によるノイズ
		float noise = 1.0f;
		if (turbulence_ > 0.0f)
		{
			time_ += context.deltaTime * turbulenceFrequency_;
			noise = 1.0f + std::sin(time_) * turbulence_;
		}

		Vector3 windForce = direction_ * strength_ * noise * context.deltaTime;

		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			particle.velocity += windForce;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Wind"; }
	int32_t GetPriority() const override { return -25; }

	void SetDirection(const Vector3& dir) { direction_ = dir.Normalize(); }
	Vector3 GetDirection() const { return direction_; }
	void SetStrength(float s) { strength_ = s; }
	float GetStrength() const { return strength_; }
	void SetTurbulence(float t) { turbulence_ = t; }
	float GetTurbulence() const { return turbulence_; }
	void SetTurbulenceFrequency(float f) { turbulenceFrequency_ = f; }
	float GetTurbulenceFrequency() const { return turbulenceFrequency_; }

private:
	Vector3 direction_ = { 1, 0, 0 };
	float strength_ = 1.0f;
	float turbulence_ = 0.0f;
	float turbulenceFrequency_ = 1.0f;
	float time_ = 0.0f;
};

/**
 * @brief フリッカーモジュール
 * アルファ値を点滅させる
 */
class FlickerModule : public IModule
{
public:
	FlickerModule(float frequency = 10.0f, float minAlpha = 0.3f, float maxAlpha = 1.0f)
		: frequency_(frequency), minAlpha_(minAlpha), maxAlpha_(maxAlpha) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float t = particle.age * frequency_;
			float alpha;
			
			if (randomPhase_)
			{
				// パーティクルIDでフェーズをオフセット
				t += static_cast<float>(particle.id) * 0.1f;
			}

			if (useNoise_)
			{
				// ノイズベース
				alpha = (std::sin(t * 2.0f) + std::sin(t * 3.7f) + 2.0f) * 0.25f;
			}
			else
			{
				// シンプルなサイン波
				alpha = (std::sin(t * std::numbers::pi_v<float> * 2.0f) + 1.0f) * 0.5f;
			}

			particle.color.w = minAlpha_ + (maxAlpha_ - minAlpha_) * alpha;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Flicker"; }
	int32_t GetPriority() const override { return 55; }

	void SetFrequency(float f) { frequency_ = f; }
	float GetFrequency() const { return frequency_; }
	void SetMinAlpha(float a) { minAlpha_ = a; }
	float GetMinAlpha() const { return minAlpha_; }
	void SetMaxAlpha(float a) { maxAlpha_ = a; }
	float GetMaxAlpha() const { return maxAlpha_; }
	void SetRandomPhase(bool r) { randomPhase_ = r; }
	bool GetRandomPhase() const { return randomPhase_; }
	void SetUseNoise(bool n) { useNoise_ = n; }
	bool GetUseNoise() const { return useNoise_; }

private:
	float frequency_ = 10.0f;
	float minAlpha_ = 0.3f;
	float maxAlpha_ = 1.0f;
	bool randomPhase_ = true;
	bool useNoise_ = false;
};

/**
 * @brief アルファフェードモジュール
 * アルファ値のみをシンプルにフェード（ColorFadeの簡易版）
 */
class AlphaFadeModule : public IModule
{
public:
	AlphaFadeModule(float startAlpha = 1.0f, float endAlpha = 0.0f)
		: startAlpha_(startAlpha), endAlpha_(endAlpha) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float t = particle.NormalizedAge();
			
			// イージング適用（オプション）
			if (easeIn_ && easeOut_)
			{
				t = t * t * (3.0f - 2.0f * t); // smoothstep
			}
			else if (easeIn_)
			{
				t = t * t;
			}
			else if (easeOut_)
			{
				t = 1.0f - (1.0f - t) * (1.0f - t);
			}

			particle.color.w = startAlpha_ + (endAlpha_ - startAlpha_) * t;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "AlphaFade"; }
	int32_t GetPriority() const override { return 48; }

	void SetStartAlpha(float a) { startAlpha_ = a; }
	float GetStartAlpha() const { return startAlpha_; }
	void SetEndAlpha(float a) { endAlpha_ = a; }
	float GetEndAlpha() const { return endAlpha_; }
	void SetEaseIn(bool e) { easeIn_ = e; }
	bool GetEaseIn() const { return easeIn_; }
	void SetEaseOut(bool e) { easeOut_ = e; }
	bool GetEaseOut() const { return easeOut_; }

private:
	float startAlpha_ = 1.0f;
	float endAlpha_ = 0.0f;
	bool easeIn_ = false;
	bool easeOut_ = true;
};

/**
 * @brief 速度による回転モジュール
 * 速度に応じて回転速度を変化させる
 */
class RotationBySpeedModule : public IModule
{
public:
	RotationBySpeedModule(float rotationPerSpeed = 90.0f)
		: rotationPerSpeed_(rotationPerSpeed) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float speed = particle.velocity.Length();
			float rotationSpeed = speed * rotationPerSpeed_; // degrees per second
			
			if (speed < minSpeed_) continue;
			if (maxSpeed_ > 0.0f && speed > maxSpeed_) rotationSpeed = maxSpeed_ * rotationPerSpeed_;

			float angleRad = rotationSpeed * context.deltaTime * (std::numbers::pi_v<float> / 180.0f);

			// オイラー角（Z軸）に加算
			particle.rotation.z += angleRad;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "RotationBySpeed"; }
	int32_t GetPriority() const override { return 62; }

	void SetRotationPerSpeed(float r) { rotationPerSpeed_ = r; }
	float GetRotationPerSpeed() const { return rotationPerSpeed_; }
	void SetMinSpeed(float s) { minSpeed_ = s; }
	float GetMinSpeed() const { return minSpeed_; }
	void SetMaxSpeed(float s) { maxSpeed_ = s; }
	float GetMaxSpeed() const { return maxSpeed_; }

private:
	float rotationPerSpeed_ = 90.0f; // degrees per unit speed per second
	float minSpeed_ = 0.0f;
	float maxSpeed_ = 0.0f; // 0 = unlimited
};

/**
 * @brief 正弦波モジュール
 * 正弦波による往復運動を適用
 */
class SineWaveModule : public IModule
{
public:
	SineWaveModule(float amplitude = 1.0f, float frequency = 2.0f)
		: amplitude_(amplitude), frequency_(frequency) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float t = particle.age * frequency_ * std::numbers::pi_v<float> * 2.0f;
			
			// パーティクルIDで位相をオフセット（同期しないように）
			if (randomPhase_)
			{
				t += static_cast<float>(particle.id) * 1.7f;
			}

			float wave = std::sin(t) * amplitude_;
			
			// 軸に沿った往復運動
			particle.position.x += axis_.x * wave * context.deltaTime;
			particle.position.y += axis_.y * wave * context.deltaTime;
			particle.position.z += axis_.z * wave * context.deltaTime;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "SineWave"; }
	int32_t GetPriority() const override { return -28; }

	void SetAmplitude(float a) { amplitude_ = a; }
	float GetAmplitude() const { return amplitude_; }
	void SetFrequency(float f) { frequency_ = f; }
	float GetFrequency() const { return frequency_; }
	void SetAxis(const Vector3& axis) { axis_ = axis.Normalize(); }
	Vector3 GetAxis() const { return axis_; }
	void SetRandomPhase(bool r) { randomPhase_ = r; }
	bool GetRandomPhase() const { return randomPhase_; }

private:
	float amplitude_ = 1.0f;
	float frequency_ = 2.0f;
	Vector3 axis_ = { 1, 0, 0 }; // デフォルトはX軸
	bool randomPhase_ = true;
};

/**
 * @brief スパイラルモジュール
 * 螺旋パターンで移動
 */
class SpiralModule : public IModule
{
public:
	SpiralModule(float radius = 1.0f, float speed = 180.0f, float lift = 1.0f)
		: radius_(radius), speed_(speed), lift_(lift) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			// 角度を計算
			float angle = particle.age * speed_ * (std::numbers::pi_v<float> / 180.0f);
			
			// パーティクルごとに位相をずらす
			if (randomPhase_)
			{
				angle += static_cast<float>(particle.id) * 0.5f;
			}

			// 螺旋の増分を計算
			float radiusAtTime = radius_;
			if (expandRadius_)
			{
				radiusAtTime = radius_ * (1.0f + particle.age * expansionRate_);
			}

			// 速度を螺旋方向に設定
			float cosA = std::cos(angle);
			float sinA = std::sin(angle);
			
			// 接線方向の速度
			particle.velocity.x += (-sinA * radiusAtTime * speed_ * (std::numbers::pi_v<float> / 180.0f)) * context.deltaTime;
			particle.velocity.z += (cosA * radiusAtTime * speed_ * (std::numbers::pi_v<float> / 180.0f)) * context.deltaTime;
			particle.velocity.y += lift_ * context.deltaTime;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Spiral"; }
	int32_t GetPriority() const override { return -22; }

	void SetRadius(float r) { radius_ = r; }
	float GetRadius() const { return radius_; }
	void SetSpeed(float s) { speed_ = s; }
	float GetSpeed() const { return speed_; }
	void SetLift(float l) { lift_ = l; }
	float GetLift() const { return lift_; }
	void SetRandomPhase(bool r) { randomPhase_ = r; }
	bool GetRandomPhase() const { return randomPhase_; }
	void SetExpandRadius(bool e) { expandRadius_ = e; }
	bool GetExpandRadius() const { return expandRadius_; }
	void SetExpansionRate(float r) { expansionRate_ = r; }
	float GetExpansionRate() const { return expansionRate_; }

private:
	float radius_ = 1.0f;
	float speed_ = 180.0f; // degrees per second
	float lift_ = 1.0f;
	bool randomPhase_ = true;
	bool expandRadius_ = false;
	float expansionRate_ = 0.5f;
};

/**
 * @brief ツイストモジュール
 * 時間経過で位置をねじる
 */
class TwistModule : public IModule
{
public:
	TwistModule(float twistSpeed = 90.0f, float twistStrength = 1.0f)
		: twistSpeed_(twistSpeed), twistStrength_(twistStrength) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			// 距離に応じたねじれ量
			Vector3 relPos = particle.position - context.emitterPosition;
			float distance = heightAxis_ == 1 ? relPos.y : (heightAxis_ == 0 ? relPos.x : relPos.z);
			
			float twistAngle = distance * twistStrength_ + particle.age * twistSpeed_;
			twistAngle *= std::numbers::pi_v<float> / 180.0f;

			float cosT = std::cos(twistAngle * context.deltaTime);
			float sinT = std::sin(twistAngle * context.deltaTime);

			// Y軸周りにねじる（heightAxis_に応じて変更可能）
			float oldX = relPos.x;
			float oldZ = relPos.z;
			
			relPos.x = oldX * cosT - oldZ * sinT;
			relPos.z = oldX * sinT + oldZ * cosT;

			particle.position = context.emitterPosition + relPos;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Twist"; }
	int32_t GetPriority() const override { return -18; }

	void SetTwistSpeed(float s) { twistSpeed_ = s; }
	float GetTwistSpeed() const { return twistSpeed_; }
	void SetTwistStrength(float s) { twistStrength_ = s; }
	float GetTwistStrength() const { return twistStrength_; }
	void SetHeightAxis(int axis) { heightAxis_ = axis; } // 0=X, 1=Y, 2=Z
	int GetHeightAxis() const { return heightAxis_; }

private:
	float twistSpeed_ = 90.0f; // degrees per second
	float twistStrength_ = 1.0f;
	int heightAxis_ = 1; // Y axis by default
};
