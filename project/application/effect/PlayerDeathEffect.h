#pragma once
#include "application/GameObject/Combatable/character/player/Player.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief プレイヤー死亡時のビジュアルエフェクトクラス
 * 
 * プレイヤーキャラクターが倒されたときの演出を管理します。
 * プレイヤーモデルの縮小アニメーションと破片パーティクルを組み合わせ、
 * 印象的な死亡演出を実現します。
 * 
 * 主な機能:
 * - プレイヤーモデルの縮小アニメーション（イージング適用）
 * - 破片が飛び散るパーティクルエフェクト
 * - タイミング制御された演出の段階的展開
 * - エフェクト完了判定
 * 
 * @code
 * // 使用例
 * PlayerDeathEffect deathEffect;
 * deathEffect.Initialize(&player);
 * deathEffect.Play(2.0f);  // 2秒間のエフェクト
 * 
 * // 毎フレーム
 * deathEffect.Update();
 * if (deathEffect.IsFinished()) {
 *     // 死亡処理完了
 * }
 * @endcode
 */
class PlayerDeathEffect
{
public:
	/**
	 * @brief エフェクトの初期化
	 * 
	 * プレイヤーへの参照を保存し、破片エミッターを初期化します。
	 * プレイヤーの初期スケールを記録し、アニメーション開始点として使用します。
	 * 
	 * @param player 対象となるプレイヤーオブジェクトへのポインタ
	 */
	void Initialize(Player* player);

	/**
	 * @brief エフェクトの更新処理
	 * 
	 * 毎フレーム呼び出され、プレイヤーの縮小アニメーションとパーティクルを制御します。
	 * 一定時間経過後に破片パーティクルを開始します。
	 */
	void Update();

	/**
	 * @brief エフェクトの再生開始
	 * 
	 * 死亡エフェクトを開始し、指定された時間でアニメーションを実行します。
	 * 
	 * @param duration エフェクトの継続時間（秒）
	 */
	void Play(float duration);

	/**
	 * @brief エフェクト完了判定
	 * 
	 * エフェクトが完了したかどうかを確認します。
	 * 
	 * @return true エフェクトが完了した場合
	 * @return false まだエフェクトが実行中の場合
	 */
	bool IsFinished() const;

private:
	Player* player_ = nullptr;              ///< プレイヤーオブジェクトへの参照
	Vector3 initialScale_;                  ///< プレイヤーの初期スケール（縮小アニメーションの開始値）
	bool isParticleStarted_ = false;        ///< パーティクル開始フラグ（多重起動防止）
	bool isActive_ = false;                 ///< エフェクトのアクティブ状態
	float elapsed_ = 0.0f;                  ///< 経過時間（秒）
	float duration_ = 1.0f;                 ///< エフェクト継続時間（秒）
	std::unique_ptr<ParticleEmitter> debrisEmitter_;  ///< 破片パーティクルエミッター
};

