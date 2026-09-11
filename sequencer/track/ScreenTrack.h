#pragma once
#include "math/Vector4.h"
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
class Model;

/**
 * @brief ステージの画面（モニター）の明るさと寄りをカーブで動かすトラック
 *
 * @details 役には画面の GameObject を割り当てる。Object3d を持つものだけ動かせる。
 *          - Brightness: 色の倍率（RGB に同じ値を掛ける。アルファは今の値のまま）
 *          - Zoom: 映像の中心に寄る倍率。1 で等倍、2 で中央の半分を画面いっぱいに映す
 */
class ScreenTrack : public ITrack
{
public:
	ScreenTrack();

	TrackType GetType() const override { return TrackType::Screen; }
	const char* GetTypeName() const override { return "Screen"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override { return const_cast<ScreenTrack*>(this)->GetChannelsEndTime(); }

	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return 2; }
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;

private:
	/**
	 * @brief 役に割り当てられた画面の Model を探す
	 * @return 見つからなければ nullptr（所有しない）
	 */
	Model* ResolveModel(const BindingContext& ctx) const;

	FloatCurve brightnessCurve_;
	FloatCurve zoomCurve_;
	CurveChannel<float> brightnessChannel_{ "Brightness", &brightnessCurve_ };
	CurveChannel<float> zoomChannel_{ "Zoom", &zoomCurve_ };

	// --- 状態の退避 ---
	bool hasCapturedState_ = false;
	Vector4 capturedColor_{};
	Vector3 capturedUVScale_{};
	Vector3 capturedUVTranslate_{};
};
} // namespace KCE
