#include "sequencer/track/PostProcessTrack.h"

#include "manager/effect/PostProcessManager.h"
#include "sequencer/core/CurveSerialization.h"

namespace KCE
{
PostProcessTrack::PostProcessTrack()
{
	SetName("PostProcess Track");
	// 画面に1つしかないので役は使わない
	SetBindingRole("");
}

ICurveChannel* PostProcessTrack::GetChannel(size_t index)
{
	switch (index)
	{
	case 0:  return &exposureChannel_;
	case 1:  return &bloomIntensityChannel_;
	case 2:  return &bloomThresholdChannel_;
	case 3:  return &vignetteIntensityChannel_;
	case 4:  return &grayscaleIntensityChannel_;
	default: return nullptr;
	}
}

void PostProcessTrack::Evaluate(float time, const BindingContext& ctx)
{
	PostProcessManager* post = ctx.GetPostProcessManager();
	if (!post)
	{
		return;
	}

	if (!exposureCurve_.IsEmpty()) { post->tonemapEffect_->SetExposure(exposureCurve_.Evaluate(time)); }
	if (!bloomIntensityCurve_.IsEmpty()) { post->bloomEffect_->SetIntensity(bloomIntensityCurve_.Evaluate(time)); }
	if (!bloomThresholdCurve_.IsEmpty()) { post->bloomEffect_->SetThreshold(bloomThresholdCurve_.Evaluate(time)); }

	// 既定で無効なエフェクトは、カーブを持つ間だけ有効にする。
	// 有効化はカーブの有無だけで決まるので、純関数契約は保たれる。
	if (!vignetteIntensityCurve_.IsEmpty())
	{
		post->vignetteEffect_->SetEnabled(true);
		post->vignetteEffect_->SetIntensity(vignetteIntensityCurve_.Evaluate(time));
	}
	if (!grayscaleIntensityCurve_.IsEmpty())
	{
		post->grayscaleEffect_->SetEnabled(true);
		post->grayscaleEffect_->SetIntensity(grayscaleIntensityCurve_.Evaluate(time));
	}
}

void PostProcessTrack::CaptureState(const BindingContext& ctx)
{
	PostProcessManager* post = ctx.GetPostProcessManager();
	if (!post)
	{
		hasCapturedState_ = false;
		return;
	}

	capturedExposure_ = post->tonemapEffect_->GetExposure();
	capturedBloomIntensity_ = post->bloomEffect_->GetIntensity();
	capturedBloomThreshold_ = post->bloomEffect_->GetThreshold();
	capturedVignetteIntensity_ = post->vignetteEffect_->GetIntensity();
	capturedVignetteEnabled_ = post->vignetteEffect_->IsEnabled();
	capturedGrayscaleIntensity_ = post->grayscaleEffect_->GetIntensity();
	capturedGrayscaleEnabled_ = post->grayscaleEffect_->IsEnabled();
	hasCapturedState_ = true;
}

void PostProcessTrack::RestoreState(const BindingContext& ctx)
{
	PostProcessManager* post = ctx.GetPostProcessManager();
	if (!hasCapturedState_ || !post)
	{
		return;
	}

	post->tonemapEffect_->SetExposure(capturedExposure_);
	post->bloomEffect_->SetIntensity(capturedBloomIntensity_);
	post->bloomEffect_->SetThreshold(capturedBloomThreshold_);
	post->vignetteEffect_->SetIntensity(capturedVignetteIntensity_);
	post->vignetteEffect_->SetEnabled(capturedVignetteEnabled_);
	post->grayscaleEffect_->SetIntensity(capturedGrayscaleIntensity_);
	post->grayscaleEffect_->SetEnabled(capturedGrayscaleEnabled_);
}

bool PostProcessTrack::RecordKey(float time, const BindingContext& ctx)
{
	PostProcessManager* post = ctx.GetPostProcessManager();
	if (!post)
	{
		return false;
	}

	exposureChannel_.SetKey(time, post->tonemapEffect_->GetExposure());
	bloomIntensityChannel_.SetKey(time, post->bloomEffect_->GetIntensity());
	bloomThresholdChannel_.SetKey(time, post->bloomEffect_->GetThreshold());
	vignetteIntensityChannel_.SetKey(time, post->vignetteEffect_->GetIntensity());
	grayscaleIntensityChannel_.SetKey(time, post->grayscaleEffect_->GetIntensity());
	return true;
}

nlohmann::json PostProcessTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["exposure"] = SerializeCurve(exposureCurve_);
	json["bloomIntensity"] = SerializeCurve(bloomIntensityCurve_);
	json["bloomThreshold"] = SerializeCurve(bloomThresholdCurve_);
	json["vignetteIntensity"] = SerializeCurve(vignetteIntensityCurve_);
	json["grayscaleIntensity"] = SerializeCurve(grayscaleIntensityCurve_);
	return json;
}

bool PostProcessTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}

	DeserializeCommon(json);
	if (json.contains("exposure")) { DeserializeCurve(json["exposure"], exposureCurve_); }
	if (json.contains("bloomIntensity")) { DeserializeCurve(json["bloomIntensity"], bloomIntensityCurve_); }
	if (json.contains("bloomThreshold")) { DeserializeCurve(json["bloomThreshold"], bloomThresholdCurve_); }
	if (json.contains("vignetteIntensity")) { DeserializeCurve(json["vignetteIntensity"], vignetteIntensityCurve_); }
	if (json.contains("grayscaleIntensity")) { DeserializeCurve(json["grayscaleIntensity"], grayscaleIntensityCurve_); }
	return true;
}
} // namespace KCE
