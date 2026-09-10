#pragma once
#include "math/Quaternion.h"
#include "math/Vector3.h"
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief カメラの位置・回転・画角をカーブで駆動するトラック
 *
 * @details Phase 0 の縦切り1本で扱う唯一のトラック。
 *          回転はクォータニオンで保持し Slerp で補間する。オイラー角のまま
 *          補間するとカットをまたぐ大きな回転で必ず破綻するため（SEQUENCER_PLAN 7.4）。
 *
 *          Evaluate() は ITrack の純関数契約を守る。内部に「前フレームの値」を
 *          持たず、時刻だけから位置・回転・画角を決めてカメラに書き込む。
 */
class CameraTrack : public ITrack
{
public:
	CameraTrack();

	TrackType GetType() const override { return TrackType::Camera; }
	const char* GetTypeName() const override { return "Camera"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override;

	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

#ifdef USE_IMGUI
	void DrawInspector() override;
#endif

	// --- カーブへのアクセス ---

	Vector3Curve& GetPositionCurve() { return positionCurve_; }
	const Vector3Curve& GetPositionCurve() const { return positionCurve_; }

	QuaternionCurve& GetRotationCurve() { return rotationCurve_; }
	const QuaternionCurve& GetRotationCurve() const { return rotationCurve_; }

	FloatCurve& GetFovCurve() { return fovCurve_; }
	const FloatCurve& GetFovCurve() const { return fovCurve_; }

	/**
	 * @brief 現在のカメラの状態をキーとして打ち込む
	 *
	 * @details エディタでギズモを置いてから「キーを打つ」操作の実体。
	 *          同じ時刻に既存のキーがあれば上書きする。
	 *
	 * @param time キーを打つ時刻（秒）
	 * @param camera 状態の取得元カメラ
	 * @param withPosition 位置のキーを打つか
	 * @param withRotation 回転のキーを打つか
	 * @param withFov 画角のキーを打つか
	 */
	void AddKeyFromCamera(float time, const Camera* camera, bool withPosition = true, bool withRotation = true, bool withFov = true);

	/**
	 * @brief 指定時刻付近のキーをすべて削除する
	 * @param time 対象時刻（秒）
	 * @param tolerance 一致とみなす時刻の許容差（秒）
	 * @return 1つでも削除したら真
	 */
	bool RemoveKeysAt(float time, float tolerance);

	/**
	 * @brief キーを1つも持たないか
	 * @return 全カーブが空なら真
	 */
	bool IsEmpty() const;

private:
	/** @brief 同じ時刻の既存キーを許容差の範囲で探す。無ければ -1 */
	template<class T>
	static int FindKeyIndexAt(const Curve<T>& curve, float time, float tolerance);

	// 位置のカーブ
	Vector3Curve positionCurve_;
	// 回転のカーブ（Slerp補間）
	QuaternionCurve rotationCurve_;
	// 垂直画角のカーブ（ラジアン）
	FloatCurve fovCurve_;

	// --- 状態の退避（SEQUENCER_PLAN 3.5） ---
	// 退避済みかどうか
	bool hasCapturedState_ = false;
	Vector3 capturedPosition_{};
	Quaternion capturedRotation_ = Quaternion::Identity();
	float capturedFov_ = 0.0f;
	bool capturedUsedQuaternion_ = false;
	Vector3 capturedEuler_{};
};
} // namespace KCE
