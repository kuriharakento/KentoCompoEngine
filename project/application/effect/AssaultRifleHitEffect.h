#pragma once
// effects
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief アサルトライフル着弾エフェクトクラス
 * 
 * アサルトライフルの弾丸が敵や壁などに着弾した際の衝撃波エフェクトを管理します。
 * リング状のパーティクルが拡大しながら回転し、派手な着弾演出を実現します。
 * 
 * 主な機能:
 * - リング型パーティクルによる衝撃波表現
 * - 拡大アニメーション
 * - 回転とUVスクロールによる動的な視覚効果
 * - 一回限りの発生（バースト型）
 * 
 * @code
 * // 使用例
 * AssaultRifleHitEffect hitEffect;
 * hitEffect.Initialize();
 * hitEffect.Play(hitPosition);  // 着弾位置でエフェクト再生
 * @endcode
 */
class AssaultRifleHitEffect
{
public:
	/**
	 * @brief エフェクトの初期化
	 * 
	 * 衝撃波エミッターを初期化し、リング型パーティクルの設定を行います。
	 * 回転、スケール変化、UVスクロールなどのコンポーネントを追加します。
	 */
	void Initialize();

	/**
	 * @brief 着弾エフェクトの再生
	 * 
	 * 指定位置で一度だけ衝撃波エフェクトを発生させます。
	 * 
	 * @param position 着弾位置
	 */
	void Play(const Vector3& position);

private:
	std::unique_ptr<ParticleEmitter> impactEmitter_;  ///< 衝撃波パーティクルエミッター
};

