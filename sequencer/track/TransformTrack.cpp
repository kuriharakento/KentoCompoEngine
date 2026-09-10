#include "sequencer/track/TransformTrack.h"

#include "gameobject/base/GameObject.h"
#include "sequencer/core/CurveSerialization.h"

namespace KCE
{
TransformTrack::TransformTrack()
{
	SetName("Transform Track");
	SetBindingRole("Target");
}

ICurveChannel* TransformTrack::GetChannel(size_t index)
{
	switch (index)
	{
	case 0:  return &positionChannel_;
	case 1:  return &rotationChannel_;
	case 2:  return &scaleChannel_;
	default: return nullptr;
	}
}

void TransformTrack::Evaluate(float time, const BindingContext& ctx)
{
	GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object)
	{
		return;
	}

	// キーを持たないチャンネルには触れない（位置だけ動かすトラックでスケールを壊さないため）
	if (!positionCurve_.IsEmpty())
	{
		object->SetPosition(positionCurve_.Evaluate(time));
	}
	if (!rotationCurve_.IsEmpty())
	{
		object->SetRotation(rotationCurve_.Evaluate(time).Normalized().ToEuler());
	}
	if (!scaleCurve_.IsEmpty())
	{
		object->SetScale(scaleCurve_.Evaluate(time));
	}
}

void TransformTrack::CaptureState(const BindingContext& ctx)
{
	const GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object)
	{
		hasCapturedState_ = false;
		return;
	}

	capturedPosition_ = object->GetPosition();
	capturedRotation_ = object->GetRotation();
	capturedScale_ = object->GetScale();
	hasCapturedState_ = true;
}

void TransformTrack::RestoreState(const BindingContext& ctx)
{
	if (!hasCapturedState_)
	{
		return;
	}

	GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object)
	{
		return;
	}

	object->SetPosition(capturedPosition_);
	object->SetRotation(capturedRotation_);
	object->SetScale(capturedScale_);
}

bool TransformTrack::RecordKey(float time, const BindingContext& ctx)
{
	const GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object)
	{
		return false;
	}

	positionChannel_.SetKey(time, object->GetPosition());
	rotationChannel_.SetKey(time, Quaternion::FromEuler(object->GetRotation()));
	scaleChannel_.SetKey(time, object->GetScale());
	return true;
}

nlohmann::json TransformTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["position"] = SerializeCurve(positionCurve_);
	json["rotation"] = SerializeCurve(rotationCurve_);
	json["scale"] = SerializeCurve(scaleCurve_);
	return json;
}

bool TransformTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}

	DeserializeCommon(json);
	if (json.contains("position")) { DeserializeCurve(json["position"], positionCurve_); }
	if (json.contains("rotation")) { DeserializeCurve(json["rotation"], rotationCurve_); }
	if (json.contains("scale")) { DeserializeCurve(json["scale"], scaleCurve_); }
	return true;
}
} // namespace KCE
