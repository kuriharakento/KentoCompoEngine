#pragma once
/**
 * @file BehaviorModules.h
 * @brief パーティクルの挙動制御モジュール群
 * 
 * 物理挙動（加速度、衝突）、見た目変化（速度によるサイズ/カラー）、
 * キル条件（ゾーン）などのモジュールを定義。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <cmath>
#include <numbers>
#include <algorithm>

/**
 * @brief 加速度モジュール
 * パーティクルに定数加速度を適用する
 */
class AccelerationModule : public IModule
{
public:
	AccelerationModule(const Vector3& acceleration = { 0, 0, 0 })
		: acceleration_(acceleration) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.IsAlive())
			{
				particle.velocity += acceleration_ * context.deltaTime;
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Acceleration"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kAcceleration; }

	void SetAcceleration(const Vector3& acc) { acceleration_ = acc; }
	Vector3 GetAcceleration() const { return acceleration_; }

private:
	Vector3 acceleration_ = {};
};

/**
 * @brief カールノイズ / タービュランスモジュール
 * 3Dノイズベースの乱流をパーティクルに適用
 */
class CurlNoiseModule : public IModule
{
public:
	CurlNoiseModule(float strength = 1.0f, float frequency = 1.0f, float octaves = 3)
		: strength_(strength), frequency_(frequency), octaves_(static_cast<int>(octaves)) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			// 簡易カールノイズ計算（パーリンノイズ基盤）
			Vector3 curl = ComputeCurlNoise(particle.position, particle.age * scrollSpeed_);
			
			particle.velocity.x += curl.x * strength_ * context.deltaTime;
			particle.velocity.y += curl.y * strength_ * context.deltaTime;
			particle.velocity.z += curl.z * strength_ * context.deltaTime;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "CurlNoise"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kCurlNoise; }

	void SetStrength(float s) { strength_ = s; }
	float GetStrength() const { return strength_; }
	void SetFrequency(float f) { frequency_ = f; }
	float GetFrequency() const { return frequency_; }
	void SetOctaves(int o) { octaves_ = o; }
	int GetOctaves() const { return octaves_; }
	void SetScrollSpeed(float s) { scrollSpeed_ = s; }
	float GetScrollSpeed() const { return scrollSpeed_; }

private:
	Vector3 ComputeCurlNoise(const Vector3& pos, float time) const
	{
		float eps = 0.001f;
		Vector3 p = pos * frequency_;
		p.y += time;

		// 勾配を計算してカールを得る
		float n1 = Noise3D(p.x, p.y, p.z);
		float dnx = Noise3D(p.x + eps, p.y, p.z) - n1;
		float dny = Noise3D(p.x, p.y + eps, p.z) - n1;
		float dnz = Noise3D(p.x, p.y, p.z + eps) - n1;

		// カール計算: curl(F) = (dFz/dy - dFy/dz, dFx/dz - dFz/dx, dFy/dx - dFx/dy)
		return Vector3(dny - dnz, dnz - dnx, dnx - dny);
	}

	float Noise3D(float x, float y, float z) const
	{
		// オクターブ合成のシンプルノイズ
		float value = 0.0f;
		float amplitude = 1.0f;
		for (int i = 0; i < octaves_; ++i)
		{
			value += std::sin(x * 1.7f + y * 2.3f + z * 3.1f + static_cast<float>(i) * 1.3f) * amplitude;
			x *= 2.0f; y *= 2.0f; z *= 2.0f;
			amplitude *= 0.5f;
		}
		return value;
	}

	float strength_ = 1.0f;
	float frequency_ = 1.0f;
	int octaves_ = 3;
	float scrollSpeed_ = 1.0f;
};

/**
 * @brief 速度によるスケールモジュール
 */
class SizeBySpeedModule : public IModule
{
public:
	SizeBySpeedModule(float minSpeed = 0.0f, float maxSpeed = 10.0f, 
	                  const Vector3& minScale = { 0.5f, 0.5f, 0.5f }, 
	                  const Vector3& maxScale = { 2.0f, 2.0f, 2.0f })
		: minSpeed_(minSpeed), maxSpeed_(maxSpeed), minScale_(minScale), maxScale_(maxScale) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float speed = particle.velocity.Length();
			float t = (maxSpeed_ > minSpeed_) 
				? (std::clamp(speed, minSpeed_, maxSpeed_) - minSpeed_) / (maxSpeed_ - minSpeed_)
				: 0.0f;

			particle.scale.x = minScale_.x + (maxScale_.x - minScale_.x) * t;
			particle.scale.y = minScale_.y + (maxScale_.y - minScale_.y) * t;
			particle.scale.z = minScale_.z + (maxScale_.z - minScale_.z) * t;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "SizeBySpeed"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kSizeBySpeed; }

	void SetSpeedRange(float min, float max) { minSpeed_ = min; maxSpeed_ = max; }
	float GetMinSpeed() const { return minSpeed_; }
	float GetMaxSpeed() const { return maxSpeed_; }
	void SetScaleRange(const Vector3& min, const Vector3& max) { minScale_ = min; maxScale_ = max; }
	Vector3 GetMinScale() const { return minScale_; }
	Vector3 GetMaxScale() const { return maxScale_; }

private:
	float minSpeed_ = 0.0f;
	float maxSpeed_ = 10.0f;
	Vector3 minScale_ = { 0.5f, 0.5f, 0.5f };
	Vector3 maxScale_ = { 2.0f, 2.0f, 2.0f };
};

/**
 * @brief 速度によるカラーモジュール
 */
class ColorBySpeedModule : public IModule
{
public:
	ColorBySpeedModule(float minSpeed = 0.0f, float maxSpeed = 10.0f,
	                   const Vector4& minColor = { 1, 1, 1, 1 },
	                   const Vector4& maxColor = { 1, 0, 0, 1 })
		: minSpeed_(minSpeed), maxSpeed_(maxSpeed), minColor_(minColor), maxColor_(maxColor) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			float speed = particle.velocity.Length();
			float t = (maxSpeed_ > minSpeed_)
				? (std::clamp(speed, minSpeed_, maxSpeed_) - minSpeed_) / (maxSpeed_ - minSpeed_)
				: 0.0f;

			particle.color.x = minColor_.x + (maxColor_.x - minColor_.x) * t;
			particle.color.y = minColor_.y + (maxColor_.y - minColor_.y) * t;
			particle.color.z = minColor_.z + (maxColor_.z - minColor_.z) * t;
			particle.color.w = minColor_.w + (maxColor_.w - minColor_.w) * t;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "ColorBySpeed"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kColorBySpeed; }

	void SetSpeedRange(float min, float max) { minSpeed_ = min; maxSpeed_ = max; }
	float GetMinSpeed() const { return minSpeed_; }
	float GetMaxSpeed() const { return maxSpeed_; }
	void SetColorRange(const Vector4& min, const Vector4& max) { minColor_ = min; maxColor_ = max; }
	Vector4 GetMinColor() const { return minColor_; }
	Vector4 GetMaxColor() const { return maxColor_; }

private:
	float minSpeed_ = 0.0f;
	float maxSpeed_ = 10.0f;
	Vector4 minColor_ = { 1, 1, 1, 1 };
	Vector4 maxColor_ = { 1, 0, 0, 1 };
};

/**
 * @brief コリジョンモード
 */
enum class CollisionMode
{
	Plane,      // 平面との衝突
	World,      // ワールド空間（Y=0）
	Box         // ボックス内
};

/**
 * @brief コリジョンモジュール
 * パーティクルを平面/ワールドで衝突・反射させる
 */
class CollisionModule : public IModule
{
public:
	CollisionModule(CollisionMode mode = CollisionMode::World)
		: mode_(mode) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			switch (mode_)
			{
			case CollisionMode::World:
			case CollisionMode::Plane:
			{
				// 平面衝突 (デフォルト: Y=planeHeight_)
				if (particle.position.y <= planeHeight_)
				{
					particle.position.y = planeHeight_;
					particle.velocity.y = -particle.velocity.y * bounce_;
					// 摩擦
					particle.velocity.x *= friction_;
					particle.velocity.z *= friction_;
					
					if (killOnCollision_ && std::abs(particle.velocity.y) < 0.5f)
					{
						particle.SetAlive(false);
					}
				}
				break;
			}
			case CollisionMode::Box:
			{
				Vector3 half = boxSize_ * 0.5f;
				Vector3 center = boxCenter_;
				
				// 各軸でチェック
				if (particle.position.x < center.x - half.x) 
				{
					particle.position.x = center.x - half.x;
					particle.velocity.x = -particle.velocity.x * bounce_;
				}
				else if (particle.position.x > center.x + half.x)
				{
					particle.position.x = center.x + half.x;
					particle.velocity.x = -particle.velocity.x * bounce_;
				}
				
				if (particle.position.y < center.y - half.y)
				{
					particle.position.y = center.y - half.y;
					particle.velocity.y = -particle.velocity.y * bounce_;
				}
				else if (particle.position.y > center.y + half.y)
				{
					particle.position.y = center.y + half.y;
					particle.velocity.y = -particle.velocity.y * bounce_;
				}
				
				if (particle.position.z < center.z - half.z)
				{
					particle.position.z = center.z - half.z;
					particle.velocity.z = -particle.velocity.z * bounce_;
				}
				else if (particle.position.z > center.z + half.z)
				{
					particle.position.z = center.z + half.z;
					particle.velocity.z = -particle.velocity.z * bounce_;
				}
				break;
			}
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "Collision"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kCollision; }

	void SetMode(CollisionMode m) { mode_ = m; }
	CollisionMode GetMode() const { return mode_; }
	void SetBounce(float b) { bounce_ = b; }
	float GetBounce() const { return bounce_; }
	void SetFriction(float f) { friction_ = f; }
	float GetFriction() const { return friction_; }
	void SetPlaneHeight(float h) { planeHeight_ = h; }
	float GetPlaneHeight() const { return planeHeight_; }
	void SetBoxCenter(const Vector3& c) { boxCenter_ = c; }
	Vector3 GetBoxCenter() const { return boxCenter_; }
	void SetBoxSize(const Vector3& s) { boxSize_ = s; }
	Vector3 GetBoxSize() const { return boxSize_; }
	void SetKillOnCollision(bool k) { killOnCollision_ = k; }
	bool GetKillOnCollision() const { return killOnCollision_; }

private:
	CollisionMode mode_ = CollisionMode::World;
	float bounce_ = 0.5f;
	float friction_ = 0.9f;
	float planeHeight_ = 0.0f;
	Vector3 boxCenter_ = {};
	Vector3 boxSize_ = { 10, 10, 10 };
	bool killOnCollision_ = false;
};

/**
 * @brief キルゾーンタイプ
 */
enum class KillZoneType { Box, Sphere };

/**
 * @brief キルゾーンモジュール
 * 指定領域に入ったパーティクルを消滅させる
 */
class KillZoneModule : public IModule
{
public:
	KillZoneModule(KillZoneType type = KillZoneType::Box) : zoneType_(type) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			bool inZone = false;
			
			if (zoneType_ == KillZoneType::Box)
			{
				Vector3 half = boxSize_ * 0.5f;
				Vector3 d = particle.position - center_;
				inZone = (std::abs(d.x) <= half.x && 
				          std::abs(d.y) <= half.y && 
				          std::abs(d.z) <= half.z);
			}
			else
			{
				float dist = (particle.position - center_).Length();
				inZone = (dist <= radius_);
			}

			// killInside_ が true なら内部で死亡、false なら外部で死亡
			if (killInside_ ? inZone : !inZone)
			{
				particle.SetAlive(false);
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "KillZone"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kKillZone; }

	void SetZoneType(KillZoneType t) { zoneType_ = t; }
	KillZoneType GetZoneType() const { return zoneType_; }
	void SetCenter(const Vector3& c) { center_ = c; }
	Vector3 GetCenter() const { return center_; }
	void SetBoxSize(const Vector3& s) { boxSize_ = s; }
	Vector3 GetBoxSize() const { return boxSize_; }
	void SetRadius(float r) { radius_ = r; }
	float GetRadius() const { return radius_; }
	void SetKillInside(bool k) { killInside_ = k; }
	bool GetKillInside() const { return killInside_; }

private:
	KillZoneType zoneType_ = KillZoneType::Box;
	Vector3 center_ = {};
	Vector3 boxSize_ = { 5, 5, 5 };
	float radius_ = 5.0f;
	bool killInside_ = true; // true: ゾーン内で死亡, false: ゾーン外で死亡
};

/**
 * @brief ターゲットに向かうモジュール
 * パーティクルを指定ターゲットに向けて加速させる
 */
class SprintToTargetModule : public IModule
{
public:
	SprintToTargetModule(const Vector3& target = {}, float acceleration = 5.0f)
		: target_(target), acceleration_(acceleration) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (!particle.IsAlive()) continue;

			Vector3 toTarget = target_ - particle.position;
			float distance = toTarget.Length();
			
			if (distance < arriveRadius_)
			{
				// 到着近くで減速
				if (killOnArrive_)
				{
					particle.SetAlive(false);
				}
				continue;
			}

			// 正規化して加速
			Vector3 dir = toTarget * (1.0f / distance);
			
			// 速度比例因子（オプション）
			float speedFactor = useSpeedCurve_ 
				? (1.0f - (std::min)(1.0f, distance / maxDistance_))
				: 1.0f;
			
			float acc = acceleration_ * (1.0f + speedFactor * speedBoost_);
			
			particle.velocity.x += dir.x * acc * context.deltaTime;
			particle.velocity.y += dir.y * acc * context.deltaTime;
			particle.velocity.z += dir.z * acc * context.deltaTime;
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Update; }
	const char* GetName() const override { return "SprintToTarget"; }
	int32_t GetPriority() const override { return ParticleModulePriority::kSprintToTarget; }

	void SetTarget(const Vector3& t) { target_ = t; }
	Vector3 GetTarget() const { return target_; }
	void SetAcceleration(float a) { acceleration_ = a; }
	float GetAcceleration() const { return acceleration_; }
	void SetArriveRadius(float r) { arriveRadius_ = r; }
	float GetArriveRadius() const { return arriveRadius_; }
	void SetKillOnArrive(bool k) { killOnArrive_ = k; }
	bool GetKillOnArrive() const { return killOnArrive_; }
	void SetMaxDistance(float d) { maxDistance_ = d; }
	float GetMaxDistance() const { return maxDistance_; }
	void SetSpeedBoost(float b) { speedBoost_ = b; }
	float GetSpeedBoost() const { return speedBoost_; }
	void SetUseSpeedCurve(bool u) { useSpeedCurve_ = u; }
	bool GetUseSpeedCurve() const { return useSpeedCurve_; }

private:
	Vector3 target_ = {};
	float acceleration_ = 5.0f;
	float arriveRadius_ = 0.5f;
	bool killOnArrive_ = false;
	float maxDistance_ = 10.0f;
	float speedBoost_ = 1.0f;
	bool useSpeedCurve_ = false;
};
