#pragma once
/**
 * @file SpawnShapeModules.h
 * @brief スポーン形状モジュール
 * 
 * Point, Sphere, Circle, Box, Cone, Lineなど
 * 形状ベースのパーティクル発生位置制御。
 */
#include "effects/particle/module/IModule.h"
#include "effects/particle/module/ModulePriorities.h"
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"
#include "math/MathUtils.h"
#include <cmath>
#include <numbers>

/**
 * @brief スポーン形状タイプ
 */
enum class SpawnShapeType
{
	Point,      // 点
	Sphere,     // 球体
	Circle,     // 円（XZ平面）
	Box,        // ボックス
	Cone,       // コーン
	Line        // ライン
};

/**
 * @brief スポーン位置モード
 */
enum class SpawnLocation
{
	Volume,     // 内部全体（デフォルト）
	Surface,    // 表面のみ
	Edge        // エッジのみ（Box用）
};

/**
 * @brief スポーン形状モジュール
 * パーティクルのスポーン位置と初期速度を形状に基づいて設定
 */
class SpawnShapeModule : public IModule
{
public:
	SpawnShapeModule(SpawnShapeType type = SpawnShapeType::Point)
		: shapeType_(type) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f && particle.IsAlive())
			{
				Vector3 offset = {};
				Vector3 direction = { 0, 1, 0 };

				switch (shapeType_)
				{
				case SpawnShapeType::Point:
					offset = {};
					break;

				case SpawnShapeType::Sphere:
				{
					// 球面上のランダムな点
					float theta = MathUtils::RandomFloat(0, 2.0f * std::numbers::pi_v<float>);
					float phi = MathUtils::RandomFloat(0, std::numbers::pi_v<float>);
					float r;
					if (spawnLocation_ == SpawnLocation::Surface)
					{
						r = outerRadius_; // 表面のみ
					}
					else
					{
						r = MathUtils::RandomFloat(innerRadius_, outerRadius_);
					}
					offset.x = r * std::sin(phi) * std::cos(theta);
					offset.y = r * std::cos(phi);
					offset.z = r * std::sin(phi) * std::sin(theta);
					if (emitFromSurface_ && offset.Length() > 0.001f)
					{
						direction = offset.Normalize();
					}
					break;
				}

				case SpawnShapeType::Circle:
				{
					// XZ平面上の円
					float angle = MathUtils::RandomFloat(0, 2.0f * std::numbers::pi_v<float>);
					float r;
					if (spawnLocation_ == SpawnLocation::Surface || spawnLocation_ == SpawnLocation::Edge)
					{
						r = outerRadius_; // 縁のみ
					}
					else
					{
						r = MathUtils::RandomFloat(innerRadius_, outerRadius_);
					}
					offset.x = r * std::cos(angle);
					offset.y = 0;
					offset.z = r * std::sin(angle);
					if (emitFromSurface_)
					{
						direction = Vector3(std::cos(angle), 0, std::sin(angle));
					}
					break;
				}

				case SpawnShapeType::Box:
				{
					// boxSize_はハーフサイズとして扱う
					Vector3 half = boxSize_;
					
					if (spawnLocation_ == SpawnLocation::Edge)
					{
						// エッジ上のみ（12本のエッジからランダムに選択）
						int edge = rand() % 12;
						float t = MathUtils::RandomFloat(0.0f, 1.0f);
						// エッジの長さはフルサイズ（half * 2）
						Vector3 fullSize = { half.x * 2.0f, half.y * 2.0f, half.z * 2.0f };
						
						switch (edge)
						{
						// 底面の4エッジ
						case 0: offset = { -half.x + t * fullSize.x, -half.y, -half.z }; break;
						case 1: offset = { half.x, -half.y, -half.z + t * fullSize.z }; break;
						case 2: offset = { half.x - t * fullSize.x, -half.y, half.z }; break;
						case 3: offset = { -half.x, -half.y, half.z - t * fullSize.z }; break;
						// 上面の4エッジ
						case 4: offset = { -half.x + t * fullSize.x, half.y, -half.z }; break;
						case 5: offset = { half.x, half.y, -half.z + t * fullSize.z }; break;
						case 6: offset = { half.x - t * fullSize.x, half.y, half.z }; break;
						case 7: offset = { -half.x, half.y, half.z - t * fullSize.z }; break;
						// 垂直の4エッジ
						case 8: offset = { -half.x, -half.y + t * fullSize.y, -half.z }; break;
						case 9: offset = { half.x, -half.y + t * fullSize.y, -half.z }; break;
						case 10: offset = { half.x, -half.y + t * fullSize.y, half.z }; break;
						case 11: offset = { -half.x, -half.y + t * fullSize.y, half.z }; break;
						}
					}
					else if (spawnLocation_ == SpawnLocation::Surface)
					{
						// 表面のみ（6面からランダムに選択）
						int face = rand() % 6;
						float u = MathUtils::RandomFloat(-1.0f, 1.0f);
						float v = MathUtils::RandomFloat(-1.0f, 1.0f);
						
						switch (face)
						{
						case 0: offset = { half.x, u * half.y, v * half.z }; direction = { 1, 0, 0 }; break;  // +X
						case 1: offset = { -half.x, u * half.y, v * half.z }; direction = { -1, 0, 0 }; break; // -X
						case 2: offset = { u * half.x, half.y, v * half.z }; direction = { 0, 1, 0 }; break;  // +Y
						case 3: offset = { u * half.x, -half.y, v * half.z }; direction = { 0, -1, 0 }; break; // -Y
						case 4: offset = { u * half.x, v * half.y, half.z }; direction = { 0, 0, 1 }; break;  // +Z
						case 5: offset = { u * half.x, v * half.y, -half.z }; direction = { 0, 0, -1 }; break; // -Z
						}
					}
					else
					{
						// Volume（内部全体）
						offset.x = MathUtils::RandomFloat(-half.x, half.x);
						offset.y = MathUtils::RandomFloat(-half.y, half.y);
						offset.z = MathUtils::RandomFloat(-half.z, half.z);
					}
					break;
				}

				case SpawnShapeType::Cone:
				{
					// コーン形状
					float angle = MathUtils::RandomFloat(0, 2.0f * std::numbers::pi_v<float>);
					float t = MathUtils::RandomFloat(0, 1.0f);
					float height = t * coneHeight_;
					float radiusAtHeight = outerRadius_ * (1.0f - t);
					offset.x = radiusAtHeight * std::cos(angle);
					offset.y = height;
					offset.z = radiusAtHeight * std::sin(angle);
					// コーンの表面方向
					float coneAngle = std::atan2(outerRadius_, coneHeight_);
					direction = Vector3(
						std::cos(angle) * std::sin(coneAngle),
						std::cos(coneAngle),
						std::sin(angle) * std::sin(coneAngle)
					);
					break;
				}

				case SpawnShapeType::Line:
				{
					// lineStart_からlineEnd_への線上のランダムな点をオフセットとして使用
					float t = MathUtils::RandomFloat(0, 1);
					offset.x = lineStart_.x + (lineEnd_.x - lineStart_.x) * t;
					offset.y = lineStart_.y + (lineEnd_.y - lineStart_.y) * t;
					offset.z = lineStart_.z + (lineEnd_.z - lineStart_.z) * t;
					break;
				}
				}

				particle.position = context.emitterPosition + offset;

				// 形状から外向きの速度を設定
				if (emitFromSurface_ && initialSpeed_ > 0)
				{
					particle.velocity.x = direction.x * initialSpeed_;
					particle.velocity.y = direction.y * initialSpeed_;
					particle.velocity.z = direction.z * initialSpeed_;
				}
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "SpawnShape"; }
	int32_t GetPriority() const override { return 15; } // InitialPositionModule(10)より後に実行

	// Setters/Getters
	void SetShapeType(SpawnShapeType type) { shapeType_ = type; }
	SpawnShapeType GetShapeType() const { return shapeType_; }

	void SetRadius(float inner, float outer) { innerRadius_ = inner; outerRadius_ = outer; }
	void SetInnerRadius(float r) { innerRadius_ = r; }
	void SetOuterRadius(float r) { outerRadius_ = r; }
	float GetInnerRadius() const { return innerRadius_; }
	float GetOuterRadius() const { return outerRadius_; }

	void SetBoxSize(const Vector3& size) { boxSize_ = size; }
	Vector3 GetBoxSize() const { return boxSize_; }

	void SetConeHeight(float h) { coneHeight_ = h; }
	float GetConeHeight() const { return coneHeight_; }

	void SetLine(const Vector3& start, const Vector3& end) { lineStart_ = start; lineEnd_ = end; }
	Vector3 GetLineStart() const { return lineStart_; }
	Vector3 GetLineEnd() const { return lineEnd_; }

	void SetEmitFromSurface(bool emit) { emitFromSurface_ = emit; }
	bool GetEmitFromSurface() const { return emitFromSurface_; }

	void SetInitialSpeed(float speed) { initialSpeed_ = speed; }
	float GetInitialSpeed() const { return initialSpeed_; }

	void SetSpawnLocation(SpawnLocation loc) { spawnLocation_ = loc; }
	SpawnLocation GetSpawnLocation() const { return spawnLocation_; }

private:
	SpawnShapeType shapeType_ = SpawnShapeType::Point;
	SpawnLocation spawnLocation_ = SpawnLocation::Volume;
	float innerRadius_ = 0.0f;
	float outerRadius_ = 1.0f;
	Vector3 boxSize_ = { 1, 1, 1 };
	float coneHeight_ = 2.0f;
	Vector3 lineStart_ = {};
	Vector3 lineEnd_ = { 0, 1, 0 };
	bool emitFromSurface_ = false;
	float initialSpeed_ = 0.0f;
};

/**
 * @brief 初期回転モジュール
 * Z軸まわりの回転をクォータニオンで設定
 */
class InitialRotationModule : public IModule
{
public:
	InitialRotationModule(const Vector3& minAngle = {}, const Vector3& maxAngle = { 360.0f, 360.0f, 360.0f })
		: minAngle_(minAngle), maxAngle_(maxAngle) {}

	void Execute(ParticleContext& context) override
	{
		for (auto& particle : *context.particles)
		{
			if (particle.age == 0.0f && particle.IsAlive())
			{
				// XYZ各軸まわりのランダム回転（度からラジアンへ変換）
				Vector3 angleDeg = MathUtils::RandomVector3(minAngle_, maxAngle_);
				
				// オイラー角 (XYZ)
				particle.rotation.x = angleDeg.x * (std::numbers::pi_v<float> / 180.0f);
				particle.rotation.y = angleDeg.y * (std::numbers::pi_v<float> / 180.0f);
				particle.rotation.z = angleDeg.z * (std::numbers::pi_v<float> / 180.0f);
			}
		}
	}

	ModulePhase GetPhase() const override { return ModulePhase::Spawn; }
	const char* GetName() const override { return "InitialRotation"; }
	int32_t GetPriority() const override { return 35; }

	void SetRotationRange(const Vector3& min, const Vector3& max) { minAngle_ = min; maxAngle_ = max; }
	Vector3 GetMinAngle() const { return minAngle_; }
	Vector3 GetMaxAngle() const { return maxAngle_; }

private:
	Vector3 minAngle_ = {};
	Vector3 maxAngle_ = { 360.0f, 360.0f, 360.0f };
};
