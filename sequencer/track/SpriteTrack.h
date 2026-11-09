#pragma once
#include <string>

#include "math/Vector3.h"
#include "math/Vector4.h"
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
class Sprite;

/**
 * @brief 2D スプライト（SpriteLayer に名前で登録された物）を動かすトラック
 *
 * @details 対象は SpriteLayer に登録された名前で探す（役は使わない）。Level の「スプライト」要素はこの名前で登録される。
 *          - Position: 画面上の位置（x, y。z は使わない）。座標は 1920x1080 の画面
 *          - Size: 大きさ（x, y。z は使わない）
 *          - Rotation: 回転（ラジアン）
 *          - Color: 色（アルファで出入りさせる）
 */
class SpriteTrack : public ITrack
{
public:
	SpriteTrack();

	TrackType GetType() const override { return TrackType::Sprite; }
	const char* GetTypeName() const override { return "Sprite"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override { return const_cast<SpriteTrack*>(this)->GetChannelsEndTime(); }

	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return 4; }
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;
	void UpdateKeyDefaults(const BindingContext& ctx) override;

	const std::string& GetTargetName() const { return targetName_; }
	void SetTargetName(const std::string& name) { targetName_ = name; }

#ifdef USE_IMGUI
	bool DrawInspector() override;
	bool DrawInspector(const BindingContext& ctx) override;
#endif

private:
	/** @return 見つからなければ nullptr（所有しない） */
	Sprite* ResolveSprite(const BindingContext& ctx) const;

	std::string targetName_;

	Vector3Curve positionCurve_;
	Vector3Curve sizeCurve_;
	FloatCurve rotationCurve_;
	Vector4Curve colorCurve_;
	CurveChannel<Vector3> positionChannel_{ "Position", &positionCurve_ };
	CurveChannel<Vector3> sizeChannel_{ "Size", &sizeCurve_, Vector3{ 100.0f, 100.0f, 0.0f } };
	CurveChannel<float> rotationChannel_{ "Rotation", &rotationCurve_ };
	CurveChannel<Vector4> colorChannel_{ "Color", &colorCurve_, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } };

	// --- 状態の退避 ---
	bool hasCapturedState_ = false;
	Vector3 capturedPosition_{};
	Vector3 capturedSize_{};
	float capturedRotation_ = 0.0f;
	Vector4 capturedColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
};
} // namespace KCE
