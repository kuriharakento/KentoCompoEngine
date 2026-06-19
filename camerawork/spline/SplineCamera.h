#pragma once

// camerawork
#include "camerawork/base/CameraWorkBase.h"
#include "camerawork/spline/SplineData.h"

class LineManager;

/**
 * @brief スプラインパスに沿って移動するカメラクラス
 * 
 * Catmull-Romスプライン補間を使用して、定義された制御点に沿って滑らかに移動するカメラ。
 * JSONファイルから制御点を読み込み可能。ループ再生や特定ターゲットへの注視にも対応。
 */
class SplineCamera : public CameraWorkBase
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
	 * スプライン上の位置を計算し、カメラを移動・回転させる。
	 */
	void Update() override;

	/**
	 * @brief スプラインカメラを開始
	 * @param speed 移動速度（0〜1の進行率/フレーム、例: 0.01f）
	 * @param loop ループ再生するかどうか
	 */
	void Start(float speed, bool loop);

	/**
	 * @brief JSONファイルから制御点を読み込み
	 * @param filePath JSONファイルのパス
	 */
	void LoadJson(const std::string& filePath);

	/**
	 * @brief ループ再生の設定
	 * @param loop ループする場合true
	 */
	void SetLoop(bool loop) { loop_ = loop; }

	/**
	 * @brief 注視ターゲットを設定
	 * @param target 注視するターゲットの座標ポインタ（nullptrで進行方向を向く）
	 */
	void SetTarget(const Vector3* target) { targetPtr_ = target; }

	/**
	 * @brief 進行方向を向くかどうかを設定
	 * @param lookFront 進行方向を向く場合true
	 */
	void SetLookFront(bool lookFront) { lookFront = lookFront; }

	/**
	 * @brief スプライン曲線をデバッグ描画
	 * 
	 * デバッグモード時にスプラインパスを線で描画する。
	 */
	void DrawSplineLine();

	/**
	 * @brief スプライン移動が終了したかを取得
	 * @return 終了している場合true（ループ時は常にfalse）
	 */
	bool IsEnd() const { return isEnd_; }

private:
	// 操作対象のカメラ
	Camera* camera_ = nullptr;
	// スプラインデータ（制御点情報）
	std::shared_ptr<SplineData> splineData_ = nullptr;
	// ライン描画用マネージャー
	LineManager* lineManager_ = nullptr;
	// 注視ターゲットのポインタ
	const Vector3* targetPtr_ = nullptr;
	// 現在の進行時間（0.0〜1.0）
	float time_ = 0.0f;
	// 移動速度（進行率/フレーム）
	float speed_ = 0.0f;
	// ループフラグ
	bool loop_ = false;
	// 進行方向を向くかどうか
	bool lookFront = true;
	// 終了フラグ
	bool isEnd_ = false;
};

