#pragma once
#include "base/Camera.h"

namespace KCE
{
/**
 * @brief カメラワークの基底クラス
 * 
 * 各種カメラワーク（Follow、Orbit、Spline、TopDown、Debug）の共通インターフェースを定義する抽象クラス。
 * すべてのカメラワーククラスはこのクラスを継承し、Initialize()とUpdate()を実装する必要がある。
 */
class CameraWorkBase
{
public:
	/**
	 * @brief カメラワークの初期化
	 * @param camera 操作対象のカメラポインタ
	 */
	virtual void Initialize(Camera* camera) = 0;

	/**
	 * @brief カメラワークの更新処理
	 * 
	 * 毎フレーム呼び出され、カメラの位置・回転を更新する。
	 */
	virtual void Update() = 0;

	/**
	 * @brief 操作対象のカメラを設定
	 * @param camera 操作対象のカメラポインタ
	 */
	void Setcamera(Camera* camera) { camera_ = camera; }

protected:
	// 操作対象のカメラ
	Camera* camera_ = nullptr;
};
} // namespace KCE
