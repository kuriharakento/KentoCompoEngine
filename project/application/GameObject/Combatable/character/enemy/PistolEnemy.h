#pragma once

#include "base/EnemyBase.h"

/**
 * @brief ピストルを持つ敵キャラクタークラス
 * 
 * ピストルで単発攻撃を行う敵キャラクターです。
 * 基本的な敵タイプで、近〜中距離での戦闘を行います。
 * 
 * 継承関係:
 * GameObject → CombatableObject → Character → EnemyBase → **PistolEnemy**
 * 
 * 主な機能:
 * - ピストルによる単発攻撃
 * - プレイヤー弾との衝突判定
 * - 被弾時の即死処理
 */
class PistolEnemy : public EnemyBase
{
public:
	PistolEnemy() : Character(GameObjectTag::Character::PistolEnemy) {}
	
	/**
	 * @brief ピストル敵の初期化
	 * 
	 * ピストルコンポーネントと衝突判定を設定します。
	 * 
	 * @param object3dCommon 3Dオブジェクト共通データ
	 * @param lightManager ライト管理クラス
	 * @param target 追跡対象（通常はプレイヤー）
	 * @param initialTransform 初期トランスフォーム（デフォルト値あり）
	 */
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform()) override;
	
	/**
	 * @brief 毎フレームの更新処理
	 */
	void Update() override;
	
	/**
	 * @brief 描画処理
	 * @param camera カメラ管理クラス
	 */
	void Draw(CameraManager* camera) override;
};

