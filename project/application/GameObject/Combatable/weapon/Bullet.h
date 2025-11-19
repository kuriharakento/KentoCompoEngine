#pragma once

#include "math/Vector3.h"
#include <memory>

#include "application/GameObject/combatable/base/CombatableObject.h"

/**
 * @brief 弾丸を表すクラス
 * 
 * プレイヤーや敵が発射する弾丸オブジェクトです。
 * 攻撃力とHP（耐久力）を持ち、衝突時にダメージを与えます。
 */
class Bullet : public CombatableObject
{
public:
	~Bullet() = default;
	
	/**
	 * @brief コンストラクタ
	 * @param tag 弾丸のタグ（プレイヤー弾 or 敵弾）
	 */
	Bullet(std::string tag) : CombatableObject(tag){}

	/**
	 * @brief 弾丸の初期化
	 * @param object3dCommon 3Dオブジェクト共通データ
	 * @param lightManager ライト管理クラス
	 * @param position 弾丸の初期位置
	 */
	void Initialize(Object3dCommon* object3dCommon, LightManager* lightManager, const Vector3& position);
	
	/**
	 * @brief 毎フレームの更新処理
	 */
	void Update();
	
	/**
	 * @brief 描画処理
	 * @param camera カメラ管理クラス
	 */
	void Draw(CameraManager* camera);
};
