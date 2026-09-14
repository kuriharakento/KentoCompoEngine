#include "sequencer/track/LightTrack.h"

#include <cmath>

#include "graphics/atmosphere/BeamRenderer.h"
#include "manager/scene/LightManager.h"
#include "sequencer/core/CurveSerialization.h"

namespace KCE
{
namespace
{
const char* KindToString(LightTrackKind kind)
{
	switch (kind)
	{
	case LightTrackKind::Point:       return "Point";
	case LightTrackKind::Directional: return "Directional";
	default:                          return "Spot";
	}
}

LightTrackKind KindFromString(const std::string& str)
{
	if (str == "Point") { return LightTrackKind::Point; }
	if (str == "Directional") { return LightTrackKind::Directional; }
	return LightTrackKind::Spot;
}

/** @brief cos の値域 [-1,1] に収めてから角度へ戻す */
float CosToAngle(float cosValue)
{
	cosValue = cosValue < -1.0f ? -1.0f : (cosValue > 1.0f ? 1.0f : cosValue);
	return std::acos(cosValue);
}
} // namespace

LightTrack::LightTrack()
{
	SetName("Light Track");
	SetBindingRole("Light");
}

size_t LightTrack::GetChannelCount() const
{
	// 種類ごとに意味を持つカーブだけを見せる。平行光源にコーン角は無い
	switch (kind_)
	{
	case LightTrackKind::Spot:  return 4; // 色・強さ・コーン角・ビーム
	case LightTrackKind::Point: return 3; // 色・強さ・半径
	default:                    return 2; // 色・強さ
	}
}

ICurveChannel* LightTrack::GetChannel(size_t index)
{
	if (index == 0) { return &colorChannel_; }
	if (index == 1) { return &intensityChannel_; }
	if (index == 2 && kind_ == LightTrackKind::Spot) { return &coneAngleChannel_; }
	if (index == 2 && kind_ == LightTrackKind::Point) { return &radiusChannel_; }
	if (index == 3 && kind_ == LightTrackKind::Spot) { return &beamChannel_; }
	return nullptr;
}

bool LightTrack::ResolveLight(const BindingContext& ctx, std::string& outName) const
{
	LightManager* lightManager = ctx.GetLightManager();
	if (!lightManager)
	{
		return false;
	}

	if (kind_ == LightTrackKind::Directional)
	{
		outName.clear();
		return true;
	}

	const std::string& name = ctx.GetLightName(GetBindingRole());
	if (name.empty())
	{
		return false;
	}

	// Get* は名前が無いと先頭要素を返す（空なら未定義動作）ので、必ず先に存在を確かめる
	const bool exists = (kind_ == LightTrackKind::Spot)
		? lightManager->GetSpotLights().count(name) != 0
		: lightManager->GetPointLights().count(name) != 0;
	if (!exists)
	{
		return false;
	}

	outName = name;
	return true;
}

void LightTrack::Evaluate(float time, const BindingContext& ctx)
{
	std::string name;
	if (!ResolveLight(ctx, name))
	{
		return;
	}
	LightManager* lightManager = ctx.GetLightManager();

	switch (kind_)
	{
	case LightTrackKind::Spot:
		if (!colorCurve_.IsEmpty()) { lightManager->SetSpotLightColor(name, colorCurve_.Evaluate(time)); }
		if (!intensityCurve_.IsEmpty()) { lightManager->SetSpotLightIntensity(name, intensityCurve_.Evaluate(time)); }
		if (!coneAngleCurve_.IsEmpty()) { lightManager->SetSpotLightCosAngle(name, std::cos(coneAngleCurve_.Evaluate(time))); }
		// ビームはカーブを持つ間だけ出す。有効化はカーブの有無だけで決まるので純関数契約は保たれる
		if (!beamCurve_.IsEmpty())
		{
			if (BeamRenderer* beam = ctx.GetBeamRenderer())
			{
				beam->SetBeamEnabled(name, true);
				beam->SetBeamScale(name, beamCurve_.Evaluate(time));
			}
		}
		break;

	case LightTrackKind::Point:
		if (!colorCurve_.IsEmpty()) { lightManager->SetPointLightColor(name, colorCurve_.Evaluate(time)); }
		if (!intensityCurve_.IsEmpty()) { lightManager->SetPointLightIntensity(name, intensityCurve_.Evaluate(time)); }
		if (!radiusCurve_.IsEmpty()) { lightManager->SetPointLightRadius(name, radiusCurve_.Evaluate(time)); }
		break;

	case LightTrackKind::Directional:
	{
		DirectionalLight light = lightManager->GetDirectionalLight();
		if (!colorCurve_.IsEmpty()) { light.color = colorCurve_.Evaluate(time); }
		if (!intensityCurve_.IsEmpty()) { light.intensity = intensityCurve_.Evaluate(time); }
		lightManager->SetDirectionalLight(light);
		break;
	}
	}
}

void LightTrack::CaptureState(const BindingContext& ctx)
{
	hasCapturedState_ = false;

	std::string name;
	if (!ResolveLight(ctx, name))
	{
		return;
	}
	LightManager* lightManager = ctx.GetLightManager();

	switch (kind_)
	{
	case LightTrackKind::Spot:
		capturedColor_ = lightManager->GetSpotLightColor(name);
		capturedIntensity_ = lightManager->GetSpotLightIntensity(name);
		capturedCosAngle_ = lightManager->GetSpotLightCosAngle(name);
		if (BeamRenderer* beam = ctx.GetBeamRenderer())
		{
			capturedBeamEnabled_ = beam->IsBeamEnabled(name);
			capturedBeamScale_ = beam->GetBeamScale(name);
		}
		break;
	case LightTrackKind::Point:
		capturedColor_ = lightManager->GetPointLightColor(name);
		capturedIntensity_ = lightManager->GetPointLightIntensity(name);
		capturedRadius_ = lightManager->GetPointLightRadius(name);
		break;
	case LightTrackKind::Directional:
		capturedColor_ = lightManager->GetDirectionalLight().color;
		capturedIntensity_ = lightManager->GetDirectionalLight().intensity;
		break;
	}
	hasCapturedState_ = true;
}

void LightTrack::RestoreState(const BindingContext& ctx)
{
	std::string name;
	if (!hasCapturedState_ || !ResolveLight(ctx, name))
	{
		return;
	}
	LightManager* lightManager = ctx.GetLightManager();

	switch (kind_)
	{
	case LightTrackKind::Spot:
		lightManager->SetSpotLightColor(name, capturedColor_);
		lightManager->SetSpotLightIntensity(name, capturedIntensity_);
		lightManager->SetSpotLightCosAngle(name, capturedCosAngle_);
		if (BeamRenderer* beam = ctx.GetBeamRenderer())
		{
			beam->SetBeamEnabled(name, capturedBeamEnabled_);
			beam->SetBeamScale(name, capturedBeamScale_);
		}
		break;
	case LightTrackKind::Point:
		lightManager->SetPointLightColor(name, capturedColor_);
		lightManager->SetPointLightIntensity(name, capturedIntensity_);
		lightManager->SetPointLightRadius(name, capturedRadius_);
		break;
	case LightTrackKind::Directional:
	{
		DirectionalLight light = lightManager->GetDirectionalLight();
		light.color = capturedColor_;
		light.intensity = capturedIntensity_;
		lightManager->SetDirectionalLight(light);
		break;
	}
	}
}

bool LightTrack::RecordKey(float time, const BindingContext& ctx)
{
	std::string name;
	if (!ResolveLight(ctx, name))
	{
		return false;
	}
	LightManager* lightManager = ctx.GetLightManager();

	switch (kind_)
	{
	case LightTrackKind::Spot:
		colorChannel_.SetKey(time, lightManager->GetSpotLightColor(name));
		intensityChannel_.SetKey(time, lightManager->GetSpotLightIntensity(name));
		coneAngleChannel_.SetKey(time, CosToAngle(lightManager->GetSpotLightCosAngle(name)));
		if (BeamRenderer* beam = ctx.GetBeamRenderer())
		{
			// ビームが出ていなければ 0 として記録する
			beamChannel_.SetKey(time, beam->IsBeamEnabled(name) ? beam->GetBeamScale(name) : 0.0f);
		}
		break;
	case LightTrackKind::Point:
		colorChannel_.SetKey(time, lightManager->GetPointLightColor(name));
		intensityChannel_.SetKey(time, lightManager->GetPointLightIntensity(name));
		radiusChannel_.SetKey(time, lightManager->GetPointLightRadius(name));
		break;
	case LightTrackKind::Directional:
		colorChannel_.SetKey(time, lightManager->GetDirectionalLight().color);
		intensityChannel_.SetKey(time, lightManager->GetDirectionalLight().intensity);
		break;
	}
	return true;
}

nlohmann::json LightTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["kind"] = KindToString(kind_);
	json["color"] = SerializeCurve(colorCurve_);
	json["intensity"] = SerializeCurve(intensityCurve_);
	json["coneAngle"] = SerializeCurve(coneAngleCurve_);
	json["radius"] = SerializeCurve(radiusCurve_);
	json["beam"] = SerializeCurve(beamCurve_);
	return json;
}

bool LightTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}

	DeserializeCommon(json);
	if (json.contains("kind") && json["kind"].is_string()) { kind_ = KindFromString(json["kind"].get<std::string>()); }
	if (json.contains("color")) { DeserializeCurve(json["color"], colorCurve_); }
	if (json.contains("intensity")) { DeserializeCurve(json["intensity"], intensityCurve_); }
	if (json.contains("coneAngle")) { DeserializeCurve(json["coneAngle"], coneAngleCurve_); }
	if (json.contains("radius")) { DeserializeCurve(json["radius"], radiusCurve_); }
	if (json.contains("beam")) { DeserializeCurve(json["beam"], beamCurve_); }
	return true;
}

#ifdef USE_IMGUI
bool LightTrack::DrawInspector()
{
	int kind = static_cast<int>(kind_);
	if (ImGui::Combo("ライトの種類", &kind, "スポット\0ポイント\0平行光源\0"))
	{
		kind_ = static_cast<LightTrackKind>(kind);
		return true;
	}
	return false;
}
#endif
} // namespace KCE
