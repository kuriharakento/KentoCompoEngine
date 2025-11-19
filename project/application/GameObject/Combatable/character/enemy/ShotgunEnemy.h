#pragma once
#include "base/EnemyBase.h"

/**
 * @brief ショットガンを持つ敵キャラクタークラス
 * 
 * ショットガンで散弾攻撃を行う敵キャラクターです。
 * 近距離での広範囲攻撃を得意とします。
 * 
 * 継承関係:
 * GameObject → CombatableObject → Character → EnemyBase → **ShotgunEnemy**
 * 
 * 主な機能:
 * - ショットガンによる散弾攻撃
 * - プレイヤー弾との衝突判定
 * - 被弾時の即死処理
 */
class ShotgunEnemy : public EnemyBase
{
public:
	ShotgunEnemy() : Character(GameObjectTag::Character::ShotgunEnemy) {}
	
	/**
	 * @brief ショットガン敵の初期化
	 * 
	 * ショットガンコンポーネントと衝突判定を設定します。
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

