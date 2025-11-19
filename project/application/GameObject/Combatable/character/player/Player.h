#pragma once
#include "application/GameObject/Combatable/character/base/Character.h"
#include "application/GameObject/component/base/ICollisionComponent.h"

class EnemyManager;

/**
 * @brief プレイヤーキャラクタークラス
 * 
 * プレイヤーが操作するキャラクターを表すクラスです。
 * 移動、射撃、重力などのコンポーネントを持ち、
 * 敵の弾との衝突判定を行います。
 * 
 * 継承関係:
 * GameObject → CombatableObject → Character → **Player**
 * 
 * 主な機能:
 * - 移動制御（MoveComponent）
 * - 武器による攻撃（AssaultRifleComponent）
 * - 重力物理演算（GravityPhysicsComponent）
 * - 敵弾との衝突判定
 * - 左右の腕オブジェクト管理
 */
class Player : public Character
{
public:
	~Player() = default;
	Player(std::string tag = GameObjectTag::Character::Player) : Character(tag) {}
	
	/**
	 * @brief プレイヤーの初期化
	 * 
	 * プレイヤーの初期位置、コンポーネント、子オブジェクト（左右の腕）を設定します。
	 * 
	 * @param object3dCommon 3Dオブジェクト共通データ
	 * @param lightManager ライト管理クラス
	 * @param enemyManager 敵管理クラス（移動コンポーネントで使用）
	 * @param camera カメラ管理クラス
	 */
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, EnemyManager* enemyManager, CameraManager* camera);
	
	/**
	 * @brief 毎フレームの更新処理
	 */
	void Update() override;
	
	/**
	 * @brief 描画処理
	 * @param camera カメラ管理クラス
	 */
	void Draw(CameraManager* camera) override;

private:
	/**
	 * @brief 衝突判定コンポーネントの設定
	 * 
	 * 敵の弾との衝突判定処理を設定します。
	 * スイープ判定を使用して高速移動時の衝突漏れを防ぎます。
	 * 
	 * @param collider 衝突判定コンポーネント
	 */
	void CollisionSettings(ICollisionComponent* collider) override;
};
