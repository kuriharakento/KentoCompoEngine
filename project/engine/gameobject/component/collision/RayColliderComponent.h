#pragma once
#include "engine/gameobject/component/base/ICollisionComponent.h"
#include "engine/math/Ray.h"

/**
 * @brief レイによる衝突判定コンポーネント
 * 
 * 始点、方向ベクトル、および長さを持つレイを表す
 */
class RayColliderComponent : public ICollisionComponent
{
public:
	/**
	 * @brief コンストラクタ
	 * @param owner このコンポーネントを所有するGameObject
	 */
	RayColliderComponent(GameObject* owner);

	/**
	 * @brief 初期化処理
	 */
	void Init();

	/**
	 * @brief 更新処理
	 * 
	 * GameObjectのTransform（位置・回転）に基づいて、
	 * レイの始点と方向をワールド座標系で更新します。
	 */
	void Update(GameObject* owner) override;

	/**
	 * @brief デバッグ用描画処理
	 * 
	 * 無効化されている場合は描画されません。
	 */
	void Draw();

	/**
	 * @brief ワールドスペースの方向を直接設定（回転なしで使う場合）
	 * @param dir ワールド座標系での方向ベクトル（正規化推奨）
	 */
	void SetWorldDirection(const Vector3& dir) { worldDirection_ = dir; useWorldDirection_ = true; }

	/**
	 * @brief ワールド方向の直接指定を解除し、baseDirection+所有者回転に戻す
	 */
	void ClearWorldDirection() { useWorldDirection_ = false; }

	/**
	 * @brief コライダーの種類を取得
	 * @return コライダーの種類（Ray）
	 */
	ColliderType GetColliderType() const override { return ColliderType::Ray; }

	// --- ゲッター・セッター ---

	/**
	 * @brief 更新されたワールド座標系のRayを取得
	 * @return ワールド座標系のRay空間情報
	 */
	const Ray& GetRay() const { return ray_; }

	/**
	 * @brief レイのローカルオフセット位置を設定
	 * @param offset GameObjectの中心からの座標ズレ
	 */
	void SetOffset(const Vector3& offset) { offset_ = offset; }
	
	/**
	 * @brief レイのローカル方向（基準）を設定
	 * @param direction 基準となる方向ベクトル（デフォは前方Z軸）
	 */
	void SetBaseDirection(const Vector3& direction) { baseDirection_ = direction; }

	/**
	 * @brief レイの長さを設定
	 * @param length 判定が行われる最大距離
	 */
	void SetLength(float length) { ray_.length = length; }

private:
	// ワールド座標系に変換されたレイの空間情報
	Ray ray_{};

	// 親オブジェクト中心からのオフセット
	Vector3 offset_ = { 0.0f, 0.0f, 0.0f };
	
	// ワールド座標系で直接指定する方向（useWorldDirection_がtrueの場合に有効）
	Vector3 worldDirection_ = { 0.0f, 0.0f, 1.0f };

	// ワールド方向を直接指定するかどうか
	bool useWorldDirection_ = false;

	// ローカル基準の方向ベクトル（useWorldDirection_がfalseのとき、所有者回転を合成して使う）
	Vector3 baseDirection_ = { 0.0f, 0.0f, 1.0f };
};
