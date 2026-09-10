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
 * @details 回転はクォータニオンで保持し Slerp で補間する。オイラー角のまま
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

	size_t GetChannelCount() const override { return kChannelCount; }
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;

	// --- カーブへのアクセス ---

	Vector3Curve& GetPositionCurve() { return positionCurve_; }
	const Vector3Curve& GetPositionCurve() const { return positionCurve_; }

	QuaternionCurve& GetRotationCurve() { return rotationCurve_; }
	const QuaternionCurve& GetRotationCurve() const { return rotationCurve_; }

	FloatCurve& GetFovCurve() { return fovCurve_; }
	const FloatCurve& GetFovCurve() const { return fovCurve_; }

private:
	/** @brief チャンネル数（位置・回転・画角） */
	static constexpr size_t kChannelCount = 3;

	Vector3Curve positionCurve_;
	QuaternionCurve rotationCurve_;
	// 垂直画角（ラジアン）
	FloatCurve fovCurve_;

	// カーブをエディタへ見せる窓口。カーブの実体はこのクラスが持つ
	CurveChannel<Vector3> positionChannel_{ "Position", &positionCurve_ };
	CurveChannel<Quaternion> rotationChannel_{ "Rotation", &rotationCurve_ };
	CurveChannel<float> fovChannel_{ "FOV", &fovCurve_ };

	// --- 状態の退避（SEQUENCER_PLAN 3.5） ---
	bool hasCapturedState_ = false;
	Vector3 capturedPosition_{};
	Quaternion capturedRotation_ = Quaternion::Identity();
	float capturedFov_ = 0.0f;
	bool capturedUsedQuaternion_ = false;
	Vector3 capturedEuler_{};
};
} // namespace KCE
