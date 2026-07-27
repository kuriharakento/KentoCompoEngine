#pragma once
// camerawork
#include "camerawork/base/CameraWorkBase.h"

namespace KCE
{
/**
 * @brief ターゲット追従カメラクラス
 *
 * 指定したターゲットを追従し、マウス操作でターゲット周りを回転できるカメラ。
 * ターゲットとの距離を維持しながら、水平・垂直方向に回転可能。
 * ピッチ角（垂直方向）は制限付きで、真上や真下には向かない。
 */
class FollowCamera : public CameraWorkBase
{
public:
	FollowCamera() = default;

	/**
	 * @brief カメラワークの初期化
	 * @param camera 操作対象のカメラポインタ
	 */
	void Initialize(Camera* camera) override;

	/**
	 * @brief 追従カメラを開始
	 * @param target 追従するターゲットの座標ポインタ
	 * @param distance ターゲットとの距離
	 * @param sensitivity マウス感度（デフォルト: 0.1f）
	 */
	void Start(const Vector3* target, float distance, float sensitivity = 0.1f);

	/**
	 * @brief 毎フレームの更新処理
	 *
	 * マウス入力に基づいてカメラを回転し、ターゲットを追従する。
	 */
	void Update() override;

	/**
	 * @brief カメラがアクティブかどうかを取得
	 * @return アクティブならtrue
	 */
	bool IsActive() const { return isActive_; }

	/**
	 * @brief 追従カメラを停止
	 */
	void Stop() { isActive_ = false; }

private:
	// 追従するターゲットの座標ポインタ
	const Vector3* target_ = nullptr;
	// ターゲットとの距離
	float distance_ = 10.0f;
	// マウス感度係数
	float sensitivity_ = 0.1f;
	// 水平方向の角度（度）
	float yaw_ = 0.0f;
	// 垂直方向の角度（度）
	float pitch_ = 0.0f;
	// カメラがアクティブかどうか
	bool isActive_ = false;
	// 操作対象のカメラ
	Camera* camera_ = nullptr;
};
} // namespace KCE
