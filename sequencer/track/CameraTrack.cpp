#include "sequencer/track/CameraTrack.h"

#include <algorithm>

#include "base/Camera.h"
#include "sequencer/core/CurveSerialization.h"

namespace KCE
{
CameraTrack::CameraTrack()
{
	SetName("Camera Track");
	SetBindingRole("MainCam");
}

ICurveChannel* CameraTrack::GetChannel(size_t index)
{
	switch (index)
	{
	case 0:  return &positionChannel_;
	case 1:  return &rotationChannel_;
	case 2:  return &fovChannel_;
	default: return nullptr;
	}
}

void CameraTrack::Evaluate(float time, const BindingContext& ctx)
{
	Camera* camera = ctx.GetCamera(GetBindingRole());
	if (!camera)
	{
		// 役に実体が割り当てられていない。編集中は日常的に起きるので何もせず返る。
		return;
	}

	// キーを持たないチャンネルには触れない。
	// 触れてしまうと「位置だけ演出する」トラックが画角を初期値に戻してしまう。
	if (!positionCurve_.IsEmpty())
	{
		camera->SetTranslate(positionCurve_.Evaluate(time));
	}
	if (!rotationCurve_.IsEmpty())
	{
		camera->SetRotateQuaternion(rotationCurve_.Evaluate(time).Normalized());
	}
	if (!fovCurve_.IsEmpty())
	{
		camera->SetFovY(fovCurve_.Evaluate(time));
	}
}

float CameraTrack::GetEndTime() const
{
	return (std::max)({ positionCurve_.GetEndTime(), rotationCurve_.GetEndTime(), fovCurve_.GetEndTime() });
}

void CameraTrack::CaptureState(const BindingContext& ctx)
{
	const Camera* camera = ctx.GetCamera(GetBindingRole());
	if (!camera)
	{
		hasCapturedState_ = false;
		return;
	}

	capturedPosition_ = camera->GetTranslate();
	capturedRotation_ = camera->GetRotateQuaternion();
	capturedFov_ = camera->GetFovY();
	// 元がオイラー角モードだったなら、復元時もオイラー角モードに戻す
	capturedUsedQuaternion_ = camera->IsUsingQuaternionRotation();
	capturedEuler_ = camera->GetRotate();
	hasCapturedState_ = true;
}

void CameraTrack::RestoreState(const BindingContext& ctx)
{
	if (!hasCapturedState_)
	{
		return;
	}

	Camera* camera = ctx.GetCamera(GetBindingRole());
	if (!camera)
	{
		return;
	}

	camera->SetTranslate(capturedPosition_);
	if (capturedUsedQuaternion_)
	{
		camera->SetRotateQuaternion(capturedRotation_);
	}
	else
	{
		camera->SetRotate(capturedEuler_);
	}
	camera->SetFovY(capturedFov_);
}

bool CameraTrack::RecordKey(float time, const BindingContext& ctx)
{
	const Camera* camera = ctx.GetCamera(GetBindingRole());
	if (!camera)
	{
		return false;
	}

	positionChannel_.SetKey(time, camera->GetTranslate());
	rotationChannel_.SetKey(time, camera->GetRotateQuaternion());
	fovChannel_.SetKey(time, camera->GetFovY());
	return true;
}

nlohmann::json CameraTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["position"] = SerializeCurve(positionCurve_);
	json["rotation"] = SerializeCurve(rotationCurve_);
	json["fov"] = SerializeCurve(fovCurve_);
	return json;
}

bool CameraTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}

	DeserializeCommon(json);

	// 個々のカーブが欠けていても読み込みは続行する。
	// 壊れたデータでエディタが開けなくなるのを避けるため。
	if (json.contains("position"))
	{
		DeserializeCurve(json["position"], positionCurve_);
	}
	if (json.contains("rotation"))
	{
		DeserializeCurve(json["rotation"], rotationCurve_);
	}
	if (json.contains("fov"))
	{
		DeserializeCurve(json["fov"], fovCurve_);
	}

	return true;
}
} // namespace KCE
