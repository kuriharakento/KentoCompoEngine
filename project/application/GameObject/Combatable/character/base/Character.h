#pragma once

#include "application/GameObject/combatable/base/CombatableObject.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "math/Vector3.h"

/**
 * @brief キャラクターの基底クラス
 * 
 * プレイヤーと敵キャラクターに共通する機能を提供します。
 * ステータス管理、無敵時間、操作可否、接地判定などの基本機能を持ちます。
 * 
 * 継承関係:
 * GameObject → CombatableObject → **Character** → Player / EnemyBase
 * 
 * 主な機能:
 * - 無敵時間システム（被ダメージ後の一時的無敵）
 * - 操作可否フラグ（カットシーン中の制御）
 * - 接地判定（ジャンプ制御用）
 * - 衝突判定の自動設定（サブクラスで定義）
 * 
 * @note CollisionSettingsはサブクラスでオーバーライドして衝突処理を定義します
 */
class Character : public CombatableObject
{
public:
	virtual ~Character() = default;
	explicit Character(const std::string& tag = GameObjectTag::Common::Character): CombatableObject(tag){}
	
	/**
	 * @brief 毎フレームの更新処理
	 */
	virtual void Update() override;
	
	/**
	 * @brief 描画処理
	 * @param camera カメラ管理クラス
	 */
	virtual void Draw(CameraManager* camera);
	
	/**
	 * @brief コンポーネントを追加
	 * 
	 * 衝突判定コンポーネントの場合、サブクラス固有の衝突設定を自動適用します。
	 * 
	 * @param name コンポーネント名
	 * @param comp 追加するコンポーネント
	 */
	void AddComponent(const std::string& name, std::unique_ptr<IGameObjectComponent> comp);

	/**
	 * @brief 位置を取得
	 * @return 現在の位置座標
	 */
	const Vector3& GetPosition() const { return transform_.translate; }
	
	/**
	 * @brief 位置を設定
	 * @param position 新しい位置座標
	 */
	void SetPosition(const Vector3& position) { transform_.translate = position; }
	
	/**
	 * @brief 回転を取得
	 * @return 現在の回転角度
	 */
	const Vector3& GetRotation() const { return transform_.rotate; }
	
	/**
	 * @brief 回転を設定
	 * @param rotation 新しい回転角度
	 */
	void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
	
	/**
	 * @brief スケールを取得
	 * @return 現在のスケール値
	 */
	const Vector3& GetScale() const { return transform_.scale; }
	
	/**
	 * @brief スケールを設定
	 * @param scale 新しいスケール値
	 */
	void SetScale(const Vector3& scale) { transform_.scale = scale; }

	/**
	 * @brief 無敵状態を設定
	 * 
	 * 被ダメージ後などに一時的な無敵状態を付与します。
	 * 無敵時間中はダメージを受けません。
	 * 
	 * @param duration 無敵時間（秒）
	 */
	void SetInvincible(float duration);
	
	/**
	 * @brief 無敵状態かどうかを取得
	 * @return true: 無敵状態, false: 通常状態
	 */
	bool IsInvincible() const { return isInvincible_; }

	/**
	 * @brief 操作可能状態を設定
	 * @param controllable true: 操作可能, false: 操作不可
	 */
	void SetControllable(bool controllable) { isControllable_ = controllable; }
	
	/**
	 * @brief 操作可能かどうかを取得
	 * @return true: 操作可能, false: 操作不可
	 */
	bool IsControllable() const { return isControllable_; }

	/**
	 * @brief 接地状態を設定
	 * @param grounded true: 接地, false: 空中
	 */
	void SetIsGrounded(bool grounded) { isGrounded_ = grounded; }
	
	/**
	 * @brief 接地しているかどうかを取得
	 * @return true: 接地, false: 空中
	 */
	bool IsGrounded() const { return isGrounded_; }

protected:
	// 無敵状態フラグ
	bool isInvincible_ = false;
	
	// 無敵時間の残り（秒）
	float invincibleTimer_ = 0.0f;
	
	// 操作可能フラグ（カットシーン中などで使用）
	bool isControllable_ = true;
	
	// 地面に接地しているか（ジャンプ制御用）
	bool isGrounded_ = false;

private:
	/**
	 * @brief 衝突判定コンポーネント追加時の設定
	 * 
	 * サブクラスでオーバーライドして、キャラクター固有の
	 * 衝突処理を定義します。
	 * 
	 * @param collider 追加された衝突判定コンポーネント
	 */
	virtual void CollisionSettings(ICollisionComponent* collider) {};

};
