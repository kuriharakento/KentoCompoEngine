#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "engine/ecs/Entity.h"

class ParticleEffect;

/**
 * @brief スキルの基本ルート（LV2で選択）
 */
enum class SkillRoute : uint8_t
{
	None,
	Bomb,   // ボムルート（Q: 衝撃波）
	Turret  // タレットルート（Q: 設置）
};

/**
 * @brief スキルの派生（LV3で選択、Eキーでアクティブ化）
 */
enum class SkillSpecialChoice : uint8_t
{
	None,
	// ボムルート派生
	HomingMissile,
	DecoyBomb,
	// タレットルート派生
	MissileSalvo,
	PlasmaLaser
};

/**
 * @brief プレイヤースキルの選択状態・クールタイム・チャージを管理するコンポーネント。
 */
struct SkillComponent
{
	// === ルート設定 ===
	SkillRoute route_ = SkillRoute::None;
	SkillSpecialChoice special_ = SkillSpecialChoice::None;

	// === LMB: 通常攻撃 (初期解放) ===
	bool isLmbUnlocked_ = true;
	float lmbTimer_ = 0.0f;
	static constexpr float kLmbBaseCooldown = 0.9f;
	float lmbDamageMultiplier_ = 1.0f;
	float lmbCooldownMultiplier_ = 1.0f;
	
	// --- 通常弾 (LMB) メカニズム強化 ---
	uint32_t lmbPierceCount_ = 0;
	uint32_t lmbBulletCount_ = 1;
	float lmbProjectileSpeed_ = 80.0f;

	// === Q: ベーススキル (LV2ルート依存) ===
	float baseSkillTimer_ = 0.0f;
	static constexpr float kBaseSkillCooldown = 2.0f;
	
	// --- Qスキル強化項目 ---
	float qRange_ = 15.0f; // 衝撃波の初期半径
	float qCooldownMultiplier_ = 1.0f;

	// === E: 派生スキル (LV3派生依存) ===
	float specialSkillTimer_ = 0.0f;
	static constexpr float kSpecialSkillCooldown = 5.0f;
	float eCooldownMultiplier_ = 1.0f;

	// --- Eスキル個別強化項目 ---
	uint32_t eMissileCount_ = 5;      // ホーミングミサイル本数
	float eDecoyDuration_ = 10.0f;    // デコイ持続時間
	float eSalvoDamageMult_ = 1.0f;   // サルヴォダメージ倍率
	float eLaserFireRateMult_ = 0.1f; // レーザー発射頻度倍率（大幅短縮）

	// --- 設置物管理 ---
	std::vector<EntityID> activeDecoyEntities_;
	std::vector<EntityID> activeTurretEntities_;
	uint32_t qMaxTurrets_ = 1;        // タレット最大設置数
	float qTurretFireRateMult_ = 1.0f; // タレット連射倍率

	// --- 派生状態フラグ ---
	bool isTurretBuffActive_ = false;
	float turretBuffTimer_ = 0.0f;
	static constexpr float kTurretBuffDuration = 3.0f; // 3秒間の強化（バースト性能重視）

	// === R: ビーム（敵撃破チャージ制） ===
	bool isBeamUnlocked_ = false;
	float beamCharge_ = 0.0f;
	bool isBeamReady_ = false;
	static constexpr float kBeamChargeMax = 30.0f;
	static constexpr float kChargePerKill = 1.0f;

	// ビーム再生中の管理
	ParticleEffect* activeBeamParticle_ = nullptr;
	float beamActiveTimer_ = 0.0f;
};
