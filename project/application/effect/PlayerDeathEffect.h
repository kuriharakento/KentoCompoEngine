#pragma once
#include "application/GameObject/Combatable/character/player/Player.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief プレイヤー死亡時のビジュアルエフェクトクラス
 * 
 * プレイヤーキャラクターが倒された際の視覚効果を管理します。
 * プレイヤーのスケールを徐々に縮小させながら、
 * 破片が飛散するパーティクルエフェクトを表示します。
 */
class PlayerDeathEffect
{
public:
/**
 * @brief エフェクトの初期化
 * 
 * プレイヤーへの参照を保存し、パーティクルエミッターを作成します。
 * 
 * @param player エフェクトを適用するプレイヤーへのポインタ
 */
void Initialize(Player* player);

/**
 * @brief エフェクトの更新
 * 
 * プレイヤーのスケールを徐々に縮小し、
 * 一定の進行度でパーティクルを発生させます。
 */
void Update();

/**
 * @brief エフェクトの再生開始
 * 
 * 死亡エフェクトをアクティブにして、指定時間で完了するようにします。
 * 
 * @param duration エフェクトの持続時間（秒）
 */
void Play(float duration);

/**
 * @brief エフェクトが完了したかを確認
 * 
 * @return エフェクトが完了していればtrue、実行中であればfalse
 */
bool IsFinished() const;

private:
Player* player_ = nullptr;                            ///< エフェクトを適用するプレイヤーへのポインタ
Vector3 initialScale_;                                ///< プレイヤーの初期スケール
bool isParticleStarted_ = false;                      ///< パーティクルが開始されたかのフラグ
bool isActive_ = false;                               ///< エフェクトがアクティブかのフラグ
float elapsed_ = 0.0f;                                ///< 経過時間（秒）
float duration_ = 1.0f;                               ///< エフェクトの持続時間（秒）
std::string emitterName_ = "player_death_debris";     ///< エミッター名
};
