#pragma once
#include "base/EnemyBase.h"

/**
 * @brief ナイフを装備した近接攻撃型の敵キャラクター
 * 
 * 両腕とナイフを持ち、プレイヤーに近づいて攻撃します。
 * ビヘイビアツリーによる行動制御とコリジョン判定を実装しています。
 */
class KnifeEnemy : public EnemyBase
{
public:
	/**
	 * @brief コンストラクタ
	 */
	KnifeEnemy() : Character(GameObjectTag::Character::KnifeEnemy) {}
	
	/**
	 * @brief 初期化処理
	 * @param object3dCommon 3Dオブジェクト共通データ
	 * @param lightManager ライト管理クラス
	 * @param target 追跡対象（プレイヤー）
	 * @param initialTransform 初期トランスフォーム
	 */
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, CameraManager* camera, LightManager* lightManager, GameObject* target, const Transform& initialTransform = Transform()) override;
		
	/**
	 * @brief コリジョン設定
	 * @param collider 設定するコライダーコンポーネント
	 */
	void CollisionSettings(ICollisionComponent* collider) override;
};

