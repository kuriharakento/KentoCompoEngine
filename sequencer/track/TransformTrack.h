#pragma once
#include "math/Quaternion.h"
#include "math/Vector3.h"
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief GameObject の位置・回転・スケールをカーブで駆動するトラック
 *
 * @details 役（ロール）に割り当てた GameObject を動かす。
 *          「被弾リアクション」の Target のように、同じシーケンスを
 *          別のオブジェクトに対して使い回すための基本トラック。
 *
 *          回転はクォータニオンで補間し、GameObject へはオイラー角で渡す。
 *          GameObject のオイラー角規約（X→Y→Z）は Quaternion::FromEuler / ToEuler と
 *          一致することを検証済み。
 */
class TransformTrack : public ITrack
{
public:
	TransformTrack();

	TrackType GetType() const override { return TrackType::Transform; }
	const char* GetTypeName() const override { return "Transform"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override { return const_cast<TransformTrack*>(this)->GetChannelsEndTime(); }

	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return kChannelCount; }
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;

private:
	/** @brief チャンネル数（位置・回転・スケール） */
	static constexpr size_t kChannelCount = 3;

	Vector3Curve positionCurve_;
	QuaternionCurve rotationCurve_;
	Vector3Curve scaleCurve_;

	CurveChannel<Vector3> positionChannel_{ "Position", &positionCurve_ };
	CurveChannel<Quaternion> rotationChannel_{ "Rotation", &rotationCurve_ };
	CurveChannel<Vector3> scaleChannel_{ "Scale", &scaleCurve_ };

	// --- 状態の退避 ---
	bool hasCapturedState_ = false;
	Vector3 capturedPosition_{};
	Vector3 capturedRotation_{};
	Vector3 capturedScale_{ 1.0f, 1.0f, 1.0f };
};
} // namespace KCE
