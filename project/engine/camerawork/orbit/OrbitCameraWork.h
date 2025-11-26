#pragma once
// camerawork
#include "camerawork/base/CameraWorkBase.h"
#include <time/Timer.h>

/**
 * @brief 軌道カメラワーククラス
 * 
 * 指定したターゲットの周囲を円軌道で自動回転するカメラワーク。
 * 静的な座標または動的なポインタをターゲットとして指定可能。
 * 初期角度・回転速度・半径をカスタマイズできる。
 */
class OrbitCameraWork : public CameraWorkBase
{
public:
	/**
	 * @brief カメラワークの初期化
	 * @param camera 操作対象のカメラポインタ
	 */
	void Initialize(Camera* camera) override;

	/**
	 * @brief 毎フレームの更新処理
	 * 
	 * 円軌道上でカメラを移動し、ターゲットを向くように回転を設定する。
	 */
	void Update() override;

	/**
	 * @brief 静的な座標をターゲットとして軌道カメラを開始
	 * @param target ターゲットの座標
	 * @param radius 軌道の半径
	 * @param speed 回転速度（ラジアン/秒）
	 * @param initialAngle 初期角度（ラジアン、デフォルト: 0）
	 * @param deltaType 使用するデルタタイム種別（デフォルト: DeltaTime）
	 */
	void Start(Vector3 target, float radius, float speed, float initialAngle = 0.0f, DeltaTimeType deltaType = DeltaTimeType::DeltaTime);

	/**
	 * @brief 動的なポインタをターゲットとして軌道カメラを開始
	 * @param target ターゲットの座標ポインタ
	 * @param radius 軌道の半径
	 * @param speed 回転速度（ラジアン/秒）
	 * @param initialAngle 初期角度（ラジアン、デフォルト: 0）
	 * @param deltaType 使用するデルタタイム種別（デフォルト: DeltaTime）
	 */
	void Start(const Vector3* target, float radius, float speed, float initialAngle = 0.0f, DeltaTimeType deltaType = DeltaTimeType::DeltaTime);

	/**
	 * @brief 軌道カメラを停止
	 */
	void Stop() { isActive_ = false; }

	/**
	 * @brief アクティブ状態を取得
	 * @return アクティブの場合true
	 */
	bool IsActive() const { return isActive_; }

	/**
	 * @brief アクティブ状態を設定
	 * @param active アクティブ状態
	 */
	void SetActive(bool active) { isActive_ = active; }

	/**
	 * @brief 静的なターゲット座標を設定
	 * @param target ターゲットの座標
	 */
	void SetTarget(Vector3 target) { targetValue_ = target; }

	/**
	 * @brief 動的なターゲットポインタを設定
	 * @param target ターゲットの座標ポインタ
	 */
	void SetTarget(Vector3* target) { targetPtr_ = target; }

	/**
	 * @brief カメラ位置のオフセットを設定
	 * @param offset オフセット座標
	 */
	void SetPositionOffset(const Vector3& offset) { positionOffset = offset; }

private:
	/**
	 * @brief 現在のターゲット座標を取得
	 * @return ターゲットの座標（ポインタ優先、なければ静的座標）
	 */
	Vector3 GetTarget() const { return targetPtr_ ? *targetPtr_ : targetValue_; }

private:
	// ターゲットの静的な位置
	Vector3 targetValue_ = {};
	// ターゲットの動的な位置ポインタ
	const Vector3* targetPtr_ = nullptr;
	// 座標のオフセット
	Vector3 positionOffset = {};
	// 軌道半径
	float radius_;
	// 回転速度（ラジアン/秒）
	float speed_;
	// 経過時間（現在の角度として使用）
	float time_ = 0.0f;
	// 動作フラグ
	bool isActive_ = false;

	// 時間タイプ（DeltaTime or RealDeltaTime）
	DeltaTimeType deltaType_ = DeltaTimeType::DeltaTime;
};

