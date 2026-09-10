#include "sequencer/track/CameraTrack.h"

#include <algorithm>
#include <cmath>

#include "base/Camera.h"
#include "sequencer/core/CurveSerialization.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
CameraTrack::CameraTrack()
{
	SetName("Camera Track");
	SetBindingRole("MainCam");
}

template<class T>
int CameraTrack::FindKeyIndexAt(const Curve<T>& curve, float time, float tolerance)
{
	const auto& keys = curve.GetKeys();
	for (size_t i = 0; i < keys.size(); ++i)
	{
		if (std::abs(keys[i].time - time) <= tolerance)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
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

void CameraTrack::AddKeyFromCamera(float time, const Camera* camera, bool withPosition, bool withRotation, bool withFov)
{
	if (!camera)
	{
		return;
	}

	// 同じ時刻に打ち直した場合は上書きする。わずかな浮動小数の差で
	// キーが二重に増えるのを防ぐため、1ミリ秒を一致とみなす。
	constexpr float kSameTimeTolerance = 0.001f;

	if (withPosition)
	{
		const int existing = FindKeyIndexAt(positionCurve_, time, kSameTimeTolerance);
		if (existing >= 0)
		{
			positionCurve_.GetKey(static_cast<size_t>(existing)).value = camera->GetTranslate();
		}
		else
		{
			positionCurve_.AddKey(time, camera->GetTranslate());
		}
	}

	if (withRotation)
	{
		const int existing = FindKeyIndexAt(rotationCurve_, time, kSameTimeTolerance);
		if (existing >= 0)
		{
			rotationCurve_.GetKey(static_cast<size_t>(existing)).value = camera->GetRotateQuaternion();
		}
		else
		{
			rotationCurve_.AddKey(time, camera->GetRotateQuaternion());
		}
	}

	if (withFov)
	{
		const int existing = FindKeyIndexAt(fovCurve_, time, kSameTimeTolerance);
		if (existing >= 0)
		{
			fovCurve_.GetKey(static_cast<size_t>(existing)).value = camera->GetFovY();
		}
		else
		{
			fovCurve_.AddKey(time, camera->GetFovY());
		}
	}
}

bool CameraTrack::RemoveKeysAt(float time, float tolerance)
{
	bool removed = false;

	const int positionIndex = FindKeyIndexAt(positionCurve_, time, tolerance);
	if (positionIndex >= 0)
	{
		positionCurve_.RemoveKey(static_cast<size_t>(positionIndex));
		removed = true;
	}

	const int rotationIndex = FindKeyIndexAt(rotationCurve_, time, tolerance);
	if (rotationIndex >= 0)
	{
		rotationCurve_.RemoveKey(static_cast<size_t>(rotationIndex));
		removed = true;
	}

	const int fovIndex = FindKeyIndexAt(fovCurve_, time, tolerance);
	if (fovIndex >= 0)
	{
		fovCurve_.RemoveKey(static_cast<size_t>(fovIndex));
		removed = true;
	}

	return removed;
}

bool CameraTrack::IsEmpty() const
{
	return positionCurve_.IsEmpty() && rotationCurve_.IsEmpty() && fovCurve_.IsEmpty();
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

#ifdef USE_IMGUI
void CameraTrack::DrawInspector()
{
	ImGui::Text("Binding Role: %s", GetBindingRole().c_str());
	ImGui::Text("Position keys: %zu", positionCurve_.GetKeyCount());
	ImGui::Text("Rotation keys: %zu", rotationCurve_.GetKeyCount());
	ImGui::Text("FOV keys: %zu", fovCurve_.GetKeyCount());
	ImGui::Text("End time: %.3f s", GetEndTime());
}
#endif
} // namespace KCE
