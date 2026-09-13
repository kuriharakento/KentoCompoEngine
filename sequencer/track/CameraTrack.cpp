#include "sequencer/track/CameraTrack.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#include "base/Camera.h"
#include "gameobject/base/GameObject.h"
#include "sequencer/core/CurveSerialization.h"

namespace KCE
{
namespace
{
// 役 A と B の両方が割り当てられていて、混ぜ具合のキーが無いときは真ん中を狙う
constexpr float kDefaultAimBlend = 0.5f;
// 注目点がカメラとほぼ同じ位置なら向きを決められない
constexpr float kMinAimDistance = 1.0e-4f;
// 前方向が真上・真下にほぼ重なると右方向が作れないので、代わりの上方向に切り替える
constexpr float kParallelThreshold = 0.999f;
// 度からラジアンへ（数式由来）
constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;
constexpr size_t kRoleBufferSize = 64;

/**
 * @brief 右・上・前の3軸からカメラの回転を作る
 * @details Camera は行ベクトル規約で、回転行列の行が右・上・前（前は +Z）。
 */
Quaternion MakeRotationFromBasis(const Vector3& right, const Vector3& up, const Vector3& forward)
{
	Matrix4x4 basis = MakeIdentity4x4();
	basis.m[0][0] = right.x;   basis.m[0][1] = right.y;   basis.m[0][2] = right.z;
	basis.m[1][0] = up.x;      basis.m[1][1] = up.y;      basis.m[1][2] = up.z;
	basis.m[2][0] = forward.x; basis.m[2][1] = forward.y; basis.m[2][2] = forward.z;
	return Quaternion::FromMatrix(basis);
}

/**
 * @brief eye から target を向く回転を作る
 * @return 向きを決められたら真（2点がほぼ重なっていたら偽）
 */
bool MakeLookRotation(const Vector3& eye, const Vector3& target, Quaternion& outRotation)
{
	const Vector3 toTarget = target - eye;
	if (Vector3::Length(toTarget) < kMinAimDistance)
	{
		return false;
	}
	const Vector3 forward = Vector3::Normalize(toTarget);
	const Vector3 worldUp = std::abs(forward.y) > kParallelThreshold ? Vector3{ 0.0f, 0.0f, 1.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
	const Vector3 right = Vector3::Normalize(Vector3::Cross(worldUp, forward));
	const Vector3 up = Vector3::Cross(forward, right);
	outRotation = MakeRotationFromBasis(right, up, forward);
	return true;
}

/**
 * @brief 回転を前方向の軸まわりに傾ける
 * @param degrees 傾ける角度（度）。正でカメラの右側が上がる
 */
Quaternion ApplyRoll(const Quaternion& rotation, float degrees)
{
	const Matrix4x4 basis = rotation.ToMatrix();
	const Vector3 right = { basis.m[0][0], basis.m[0][1], basis.m[0][2] };
	const Vector3 up = { basis.m[1][0], basis.m[1][1], basis.m[1][2] };
	const Vector3 forward = { basis.m[2][0], basis.m[2][1], basis.m[2][2] };
	const float radians = degrees * kDegreesToRadians;
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	return MakeRotationFromBasis(right * c + up * s, up * c - right * s, forward);
}
} // namespace

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
	case 3:  return &aimOffsetChannel_;
	case 4:  return &aimBlendChannel_;
	case 5:  return &rollChannel_;
	default: return nullptr;
	}
}

bool CameraTrack::UsesAim() const
{
	return !aimRoleA_.empty() || !aimRoleB_.empty() || !aimOffsetCurve_.IsEmpty();
}

bool CameraTrack::EvaluateAimTarget(float time, const BindingContext& ctx, Vector3& outTarget) const
{
	const Vector3 offset = aimOffsetCurve_.IsEmpty() ? Vector3{} : aimOffsetCurve_.Evaluate(time);
	// 位置は Transform トラックと同じく GetPosition を使う（親を持たない対象を想定）
	const GameObject* objectA = aimRoleA_.empty() ? nullptr : ctx.GetGameObject(aimRoleA_);
	const GameObject* objectB = aimRoleB_.empty() ? nullptr : ctx.GetGameObject(aimRoleB_);

	if (objectA && objectB)
	{
		const float blend = aimBlendCurve_.IsEmpty() ? kDefaultAimBlend : std::clamp(aimBlendCurve_.Evaluate(time), 0.0f, 1.0f);
		const Vector3 positionA = objectA->GetPosition();
		const Vector3 positionB = objectB->GetPosition();
		outTarget = positionA + (positionB - positionA) * blend + offset;
		return true;
	}
	if (objectA || objectB)
	{
		// 片方しか割り当てられていなければ、その1人を狙う
		outTarget = (objectA ? objectA : objectB)->GetPosition() + offset;
		return true;
	}
	if (!aimRoleA_.empty() || !aimRoleB_.empty())
	{
		// 役を指定しているのに割り当てが無い。編集中に外していることがあるので狙わない
		return false;
	}
	if (aimOffsetCurve_.IsEmpty())
	{
		return false;
	}
	// 役が両方空なら、ずらし量をワールドの座標として狙う
	outTarget = ctx.ApplyOriginToPoint(offset);
	return true;
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
	// 原点が設定されていれば（カットシーンを現在地で再生する場合）、その位置と向きへ移す
	if (!positionCurve_.IsEmpty())
	{
		camera->SetTranslate(ctx.ApplyOriginToPoint(positionCurve_.Evaluate(time)));
	}

	// 向きは、注目点が決まればそちらを優先し、決まらなければ回転のキーを使う
	Quaternion rotation = Quaternion::Identity();
	bool hasRotation = false;
	Vector3 aimTarget{};
	if (UsesAim() && EvaluateAimTarget(time, ctx, aimTarget))
	{
		hasRotation = MakeLookRotation(camera->GetTranslate(), aimTarget, rotation);
	}
	if (!hasRotation && !rotationCurve_.IsEmpty())
	{
		rotation = ctx.ApplyOriginToRotation(rotationCurve_.Evaluate(time).Normalized());
		hasRotation = true;
	}
	// ロールは、この評価で向きを決めたときだけ重ねる。
	// カメラの今の向きに重ねると、評価のたびに傾きが積み重なって純関数でなくなる
	if (hasRotation)
	{
		if (!rollCurve_.IsEmpty())
		{
			rotation = ApplyRoll(rotation, rollCurve_.Evaluate(time));
		}
		camera->SetRotateQuaternion(rotation);
	}

	if (!fovCurve_.IsEmpty())
	{
		camera->SetFovY(fovCurve_.Evaluate(time));
	}
}

float CameraTrack::GetEndTime() const
{
	return (std::max)({ positionCurve_.GetEndTime(), rotationCurve_.GetEndTime(), fovCurve_.GetEndTime(),
		aimOffsetCurve_.GetEndTime(), aimBlendCurve_.GetEndTime(), rollCurve_.GetEndTime() });
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

	// 注目点・混ぜ具合・ロールは今のカメラから読み取れないので、ここでは打たない
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
	json["aimRoleA"] = aimRoleA_;
	json["aimRoleB"] = aimRoleB_;
	json["aimOffset"] = SerializeCurve(aimOffsetCurve_);
	json["aimBlend"] = SerializeCurve(aimBlendCurve_);
	json["roll"] = SerializeCurve(rollCurve_);
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
	if (json.contains("aimRoleA") && json["aimRoleA"].is_string())
	{
		aimRoleA_ = json["aimRoleA"].get<std::string>();
	}
	if (json.contains("aimRoleB") && json["aimRoleB"].is_string())
	{
		aimRoleB_ = json["aimRoleB"].get<std::string>();
	}
	if (json.contains("aimOffset"))
	{
		DeserializeCurve(json["aimOffset"], aimOffsetCurve_);
	}
	if (json.contains("aimBlend"))
	{
		DeserializeCurve(json["aimBlend"], aimBlendCurve_);
	}
	if (json.contains("roll"))
	{
		DeserializeCurve(json["roll"], rollCurve_);
	}

	return true;
}

#ifdef USE_IMGUI
bool CameraTrack::DrawInspector()
{
	bool changed = false;

	ImGui::SeparatorText("Aim");
	const auto drawRole = [&changed](const char* label, std::string& role)
	{
		char buffer[kRoleBufferSize];
		std::snprintf(buffer, sizeof(buffer), "%s", role.c_str());
		if (ImGui::InputText(label, buffer, sizeof(buffer)))
		{
			role = buffer;
			changed = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("狙う GameObject の役。A と B の両方があれば Aim Blend で混ぜる（キーが無ければ真ん中）。\n両方空なら Aim Offset をワールドの座標として狙う");
		}
	};
	drawRole("Aim Role A", aimRoleA_);
	drawRole("Aim Role B", aimRoleB_);

	if (UsesAim())
	{
		ImGui::TextDisabled("注目点を使っている間は Rotation のキーを使わない");
	}
	return changed;
}
#endif
} // namespace KCE
