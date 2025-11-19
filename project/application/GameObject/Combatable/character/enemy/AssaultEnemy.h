#pragma once
#include "base/EnemyBase.h"

/**
 * @brief アサルトライフルを持つ敵キャラクタークラス
 * 
 * アサルトライフルで連射攻撃を行う敵キャラクターです。
 * 中距離での戦闘を得意とし、プレイヤーを追跡しながら攻撃します。
 * 
 * 継承関係:
 * GameObject → CombatableObject → Character → EnemyBase → **AssaultEnemy**
 * 
 * 主な機能:
 * - アサルトライフルによる連射攻撃
 * - プレイヤー追跡行動（AssaultEnemyBehavior）
 * - 重力物理演算
 * - プレイヤー弾との衝突判定
 */
class AssaultEnemy : public EnemyBase
{
public:
	AssaultEnemy() : Character(GameObjectTag::Character::AssaultEnemy) {}
	
	/**
	 * @brief アサルト敵の初期化
	 * 
	 * アサルトライフルコンポーネント、ビヘイビア、重力演算、
	 * 衝突判定を設定します。
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
	
	/**
	 * @brief 衝突判定コンポーネントの設定
	 * 
	 * プレイヤーの弾との衝突判定処理を設定します。
	 * スイープ判定を使用して高速移動時の衝突漏れを防ぎます。
	 * 
	 * @param collider 衝突判定コンポーネント
	 */
	void CollisionSettings(ICollisionComponent* collider) override;
};

