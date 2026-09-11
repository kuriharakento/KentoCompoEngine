#include "sequencer/track/ScreenTrack.h"

#include "gameobject/base/GameObject.h"
#include "graphics/3d/Object3d.h"
#include "sequencer/core/CurveSerialization.h"

namespace KCE
{
namespace
{
// これより小さい寄りは UV が発散するので切り詰める
constexpr float kMinZoom = 0.01f;
// UV の中心（0〜1 の真ん中）
constexpr float kUVCenter = 0.5f;

/** @brief 中心に寄る倍率を UV のスケールと移動に直す */
void ApplyZoom(Model& model, float zoom)
{
	const float scale = 1.0f / (zoom < kMinZoom ? kMinZoom : zoom);
	const float offset = kUVCenter * (1.0f - scale);
	model.SetUVScale({ scale, scale, 1.0f });
	model.SetUVTranslate({ offset, offset, 0.0f });
}
} // namespace

ScreenTrack::ScreenTrack()
{
	SetName("Screen Track");
	SetBindingRole("Screen");
}

ICurveChannel* ScreenTrack::GetChannel(size_t index)
{
	if (index == 0) { return &brightnessChannel_; }
	if (index == 1) { return &zoomChannel_; }
	return nullptr;
}

Model* ScreenTrack::ResolveModel(const BindingContext& ctx) const
{
	GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object)
	{
		return nullptr;
	}
	auto* object3d = dynamic_cast<Object3d*>(object->GetRenderable3d());
	return object3d ? object3d->GetModel() : nullptr;
}

void ScreenTrack::Evaluate(float time, const BindingContext& ctx)
{
	Model* model = ResolveModel(ctx);
	if (!model)
	{
		return;
	}

	if (!brightnessCurve_.IsEmpty())
	{
		const float brightness = brightnessCurve_.Evaluate(time);
		model->SetColor({ brightness, brightness, brightness, model->GetColor().w });
	}
	if (!zoomCurve_.IsEmpty())
	{
		ApplyZoom(*model, zoomCurve_.Evaluate(time));
	}
}

void ScreenTrack::CaptureState(const BindingContext& ctx)
{
	hasCapturedState_ = false;
	Model* model = ResolveModel(ctx);
	if (!model)
	{
		return;
	}
	capturedColor_ = model->GetColor();
	capturedUVScale_ = model->GetUVScale();
	capturedUVTranslate_ = model->GetUVTranslate();
	hasCapturedState_ = true;
}

void ScreenTrack::RestoreState(const BindingContext& ctx)
{
	Model* model = ResolveModel(ctx);
	if (!hasCapturedState_ || !model)
	{
		return;
	}
	model->SetColor(capturedColor_);
	model->SetUVScale(capturedUVScale_);
	model->SetUVTranslate(capturedUVTranslate_);
}

bool ScreenTrack::RecordKey(float time, const BindingContext& ctx)
{
	Model* model = ResolveModel(ctx);
	if (!model)
	{
		return false;
	}
	// 明るさは赤の値で代表させる。寄りは UV スケールの逆数
	brightnessChannel_.SetKey(time, model->GetColor().x);
	const float uvScale = model->GetUVScale().x;
	zoomChannel_.SetKey(time, uvScale > 0.0f ? 1.0f / uvScale : 1.0f);
	return true;
}

nlohmann::json ScreenTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["brightness"] = SerializeCurve(brightnessCurve_);
	json["zoom"] = SerializeCurve(zoomCurve_);
	return json;
}

bool ScreenTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}
	DeserializeCommon(json);
	if (json.contains("brightness")) { DeserializeCurve(json["brightness"], brightnessCurve_); }
	if (json.contains("zoom")) { DeserializeCurve(json["zoom"], zoomCurve_); }
	return true;
}
} // namespace KCE
