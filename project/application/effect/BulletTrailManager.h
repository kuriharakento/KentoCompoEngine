#pragma once
/**
 * @file BulletTrailManager.h
 * @brief 弾道トレイル管理マネージャー
 *
 * ParticleManagerと連携して弾丸トレイルエフェクトを管理。
 * JSONで定義されたエフェクトを使用し、弾丸ごとにエフェクトを再生。
 */
#include <unordered_map>
#include <string>
#include "math/Vector3.h"

class ParticleEffect;
struct Transform;

/**
 * @brief 弾道トレイル管理マネージャー
 *
 * ParticleManagerのエフェクト再生機能を使用して、
 * 弾丸のTransformに追従するトレイルエフェクトを管理。
 */
class BulletTrailManager
{
public:
	/**
	 * @brief シングルトンインスタンス取得
	 * @return BulletTrailManagerの参照
	 */
	static BulletTrailManager& GetInstance();

	/**
	 * @brief 初期化
	 * 
	 * JSONからエフェクト定義を読み込む。
	 */
	void Initialize();

	/**
	 * @brief 弾を登録
	 * 
	 * 弾丸のTransformに追従するトレイルエフェクトを再生。
	 * 
	 * @param bulletTransform 追従対象のTransform
	 * @return トレイルID（解除時に必要）
	 */
	uint32_t RegisterBullet(Transform* bulletTransform);

	/**
	 * @brief 弾を登録解除
	 * 
	 * トレイルエフェクトを停止する。
	 * 
	 * @param trailId 登録時に返されたID
	 */
	void UnregisterBullet(uint32_t trailId);

	/**
	 * @brief 全トレイルをクリア
	 */
	void Clear();

private:
	BulletTrailManager() = default;
	~BulletTrailManager() = default;
	BulletTrailManager(const BulletTrailManager&) = delete;
	BulletTrailManager& operator=(const BulletTrailManager&) = delete;

private:
	// エフェクト定義名
	static constexpr const char* kEffectName = "BulletTrail";
	// JSONファイルパス
	static constexpr const char* kEffectJsonPath = "./Resources/json/particle/bulletTrail.json";

	// トレイルID → ParticleEffectのマップ
	std::unordered_map<uint32_t, ParticleEffect*> activeTrails_;
	// 次に割り当てるトレイルID
	uint32_t nextId_ = 1;
	// 初期化済みフラグ
	bool initialized_ = false;
};
