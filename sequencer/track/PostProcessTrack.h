#pragma once
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief 画面全体のポストプロセスをカーブで駆動するトラック
 *
 * @details サビで露出を上げる、暗転前にビネットを絞る、回想でグレースケールに
 *          寄せる、といった「曲調に合わせたルック切り替え」に使う。
 *          画面に1つしかないので役（ロール）は使わず、
 *          PostProcessManager を BindingContext から受け取る。
 *
 *          ビネットとグレースケールは既定で無効なので、カーブを持つ間だけ
 *          有効にし、状態の復元で元に戻す。
 */
class PostProcessTrack : public ITrack
{
public:
	PostProcessTrack();

	TrackType GetType() const override { return TrackType::PostProcess; }
	const char* GetTypeName() const override { return "PostProcess"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override { return const_cast<PostProcessTrack*>(this)->GetChannelsEndTime(); }

	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return kChannelCount; }
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;

private:
	/** @brief チャンネル数 */
	static constexpr size_t kChannelCount = 5;

	FloatCurve exposureCurve_;
	FloatCurve bloomIntensityCurve_;
	FloatCurve bloomThresholdCurve_;
	FloatCurve vignetteIntensityCurve_;
	FloatCurve grayscaleIntensityCurve_;

	CurveChannel<float> exposureChannel_{ "Exposure", &exposureCurve_ };
	CurveChannel<float> bloomIntensityChannel_{ "Bloom Intensity", &bloomIntensityCurve_ };
	CurveChannel<float> bloomThresholdChannel_{ "Bloom Threshold", &bloomThresholdCurve_ };
	CurveChannel<float> vignetteIntensityChannel_{ "Vignette", &vignetteIntensityCurve_ };
	CurveChannel<float> grayscaleIntensityChannel_{ "Grayscale", &grayscaleIntensityCurve_ };

	// --- 状態の退避 ---
	bool hasCapturedState_ = false;
	float capturedExposure_ = 1.0f;
	float capturedBloomIntensity_ = 0.0f;
	float capturedBloomThreshold_ = 0.0f;
	float capturedVignetteIntensity_ = 0.0f;
	bool capturedVignetteEnabled_ = false;
	float capturedGrayscaleIntensity_ = 0.0f;
	bool capturedGrayscaleEnabled_ = false;
};
} // namespace KCE
