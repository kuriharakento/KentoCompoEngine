#pragma once
#include <string>

#include "graphics/text/TextMesh3D.h"
#include "math/Quaternion.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief 3D 空間の文字（TextMesh3D）を動かすトラック
 *
 * @details 対象は Text3DRenderer に登録された名前で探す（役は使わない）。
 *          - Position / Rotation / Scale / Color: 文字列全体の置き方と色
 *          - Reveal: 先頭から何文字目まで出すか。0 から文字数まで上げると1文字ずつ出てくる
 *          - Exit: 先頭から何文字目まで抜けさせるか
 *          出入りの動き方と、表示する文字列（空なら置き換えない）もトラックが持つ。
 */
class Text3DTrack : public ITrack
{
public:
	Text3DTrack();

	TrackType GetType() const override { return TrackType::Text3D; }
	const char* GetTypeName() const override { return "Text3D"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override { return const_cast<Text3DTrack*>(this)->GetChannelsEndTime(); }

	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return 6; }
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;

#ifdef USE_IMGUI
	bool DrawInspector() override;
	bool DrawInspector(const BindingContext& ctx) override;
#endif

private:
	/** @return 見つからなければ nullptr（所有しない） */
	TextMesh3D* ResolveMesh(const BindingContext& ctx) const;

	// Text3DRenderer に登録された名前
	std::string targetName_ = "Text3D";
	// 空でなければ、この文字列に置き換える
	std::string text_;
	TextAppearStyle style_ = TextAppearStyle::Fade;

	Vector3Curve positionCurve_;
	QuaternionCurve rotationCurve_;
	FloatCurve scaleCurve_;
	Vector4Curve colorCurve_;
	FloatCurve revealCurve_;
	FloatCurve exitCurve_;
	CurveChannel<Vector3> positionChannel_{ "Position", &positionCurve_ };
	CurveChannel<Quaternion> rotationChannel_{ "Rotation", &rotationCurve_ };
	CurveChannel<float> scaleChannel_{ "Scale", &scaleCurve_ };
	CurveChannel<Vector4> colorChannel_{ "Color", &colorCurve_ };
	CurveChannel<float> revealChannel_{ "Reveal", &revealCurve_ };
	CurveChannel<float> exitChannel_{ "Exit", &exitCurve_ };

	// --- 状態の退避 ---
	bool hasCapturedState_ = false;
	std::string capturedText_;
	TextMesh3D::Params capturedParams_{};
};
} // namespace KCE
