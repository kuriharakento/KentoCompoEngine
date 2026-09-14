#include "sequencer/track/CameraTrack.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#include "base/Camera.h"
#include "gameobject/base/GameObject.h"
#include "graphics/postfx/DepthOfFieldRenderer.h"
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
constexpr float kSecondsPerMinute = 60.0f;
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
// 小節の拍数（4/4 拍子前提。エディタの小節線と同じ）
constexpr int kBeatsPerBar = 4;
constexpr int kBeatsPerTwoBeats = 2;
// 拍の揺れで画角を狭めても、これより小さくはしない（度）
constexpr float kMinFovDegrees = 1.0f;
// 拍の揺れの戻りの時定数の範囲（秒）。0 に近いと割り算が暴れる
constexpr float kMinBeatShakeDecay = 0.01f;
constexpr float kMaxBeatShakeDecay = 1.0f;
constexpr float kBeatShakeDecayDragSpeed = 0.005f;
// 手持ちの揺れの速さの範囲（Hz）
constexpr float kMinHandheldFrequency = 0.05f;
constexpr float kMaxHandheldFrequency = 5.0f;
constexpr float kHandheldFrequencyDragSpeed = 0.01f;
// 手持ちのロールは首振りより控えめにする
constexpr float kHandheldRollScale = 0.5f;
// 自動フレーミングの余白の範囲（m）
constexpr float kMaxFrameMargin = 50.0f;
constexpr float kFrameMarginDragSpeed = 0.05f;

/**
 * @brief 手持ちの揺れの1成分（正弦波）
 */
struct HandheldWave
{
	float frequencyScale; // 基準の速さに掛ける倍率
	float phase;		  // 位相（ラジアン）
	float weight;		  // 混ぜる重み
};

// 軸ごとに周波数の違う2つの波を足して、繰り返しが目立たないようにする。
// 倍率は軸どうし・成分どうしで周期がそろわない値にしてある（数式由来の係数）
constexpr HandheldWave kHandheldWaves[3][2] = {
	{ { 1.00f, 0.0f, 0.65f }, { 2.31f, 1.7f, 0.35f } }, // 縦の首振り
	{ { 0.83f, 2.4f, 0.65f }, { 1.97f, 0.6f, 0.35f } }, // 横の首振り
	{ { 0.71f, 4.1f, 0.65f }, { 1.63f, 3.3f, 0.35f } }, // 傾き
};
constexpr size_t kHandheldPitchAxis = 0;
constexpr size_t kHandheldYawAxis = 1;
constexpr size_t kHandheldRollAxis = 2;

const char* kBeatDivisionNames[] = { "beat", "twoBeats", "bar" };

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

/**
 * @brief ベクトルを軸まわりに回す（ロドリゲスの回転公式）
 */
Vector3 RotateAroundAxis(const Vector3& v, const Vector3& axis, float radians)
{
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	return v * c + Vector3::Cross(axis, v) * s + axis * (Vector3::Dot(axis, v) * (1.0f - c));
}

/**
 * @brief 回転に、カメラから見た横と縦の首振りを重ねる
 * @param pitchDegrees 縦の首振り（度）
 * @param yawDegrees 横の首振り（度）
 */
Quaternion ApplyLocalTilt(const Quaternion& rotation, float pitchDegrees, float yawDegrees)
{
	const Matrix4x4 basis = rotation.ToMatrix();
	Vector3 right = { basis.m[0][0], basis.m[0][1], basis.m[0][2] };
	Vector3 up = { basis.m[1][0], basis.m[1][1], basis.m[1][2] };
	Vector3 forward = { basis.m[2][0], basis.m[2][1], basis.m[2][2] };

	// 横の首振りはカメラの上方向まわり、縦の首振りは振った後の右方向まわり
	const float yaw = yawDegrees * kDegreesToRadians;
	right = RotateAroundAxis(right, up, yaw);
	forward = RotateAroundAxis(forward, up, yaw);
	const float pitch = pitchDegrees * kDegreesToRadians;
	up = RotateAroundAxis(up, right, pitch);
	forward = RotateAroundAxis(forward, right, pitch);
	return MakeRotationFromBasis(right, up, forward);
}

/**
 * @brief 手持ちの揺れの1軸ぶんを求める
 * @return おおむね -1〜1
 */
float EvaluateHandheldAxis(const HandheldWave (&waves)[2], float time, float frequency)
{
	float value = 0.0f;
	for (const HandheldWave& wave : waves)
	{
		value += wave.weight * std::sin(kTwoPi * frequency * wave.frequencyScale * time + wave.phase);
	}
	return value;
}

int GetBeatsPerShake(CameraTrack::BeatDivision division)
{
	switch (division)
	{
	case CameraTrack::BeatDivision::EveryTwoBeats: return kBeatsPerTwoBeats;
	case CameraTrack::BeatDivision::EveryBar:      return kBeatsPerBar;
	default:                                       return 1;
	}
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
	case 6:  return &beatShakeChannel_;
	case 7:  return &handheldChannel_;
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

float CameraTrack::EvaluateBeatKick(float time, const BindingContext& ctx) const
{
	const float bpm = ctx.GetBpm();
	if (bpm <= 0.0f || beatShakeCurve_.IsEmpty())
	{
		return 0.0f;
	}
	// 1拍目より前は揺らさない
	const float sinceFirstBeat = time - ctx.GetBeatOffset();
	if (sinceFirstBeat < 0.0f)
	{
		return 0.0f;
	}
	// 直近の拍からの経過で、はねた画角が指数で戻る
	const float interval = kSecondsPerMinute / bpm * static_cast<float>(GetBeatsPerShake(beatDivision_));
	const float sinceBeat = std::fmod(sinceFirstBeat, interval);
	const float decay = (std::max)(beatShakeDecay_, kMinBeatShakeDecay);
	return beatShakeCurve_.Evaluate(time) * std::exp(-sinceBeat / decay);
}

bool CameraTrack::EvaluateFramedPosition(float time, const BindingContext& ctx, const Vector3& keyedPosition, float fovY, float aspectRatio, Vector3& outPosition, Vector3& outCenter) const
{
	const GameObject* objectA = aimRoleA_.empty() ? nullptr : ctx.GetGameObject(aimRoleA_);
	const GameObject* objectB = aimRoleB_.empty() ? nullptr : ctx.GetGameObject(aimRoleB_);
	if (!objectA && !objectB)
	{
		return false;
	}

	// 片方しか割り当てられていなければ、その1人を余白の分だけ包む
	const Vector3 positionA = (objectA ? objectA : objectB)->GetPosition();
	const Vector3 positionB = (objectB ? objectB : objectA)->GetPosition();
	const Vector3 offset = aimOffsetCurve_.IsEmpty() ? Vector3{} : aimOffsetCurve_.Evaluate(time);
	outCenter = (positionA + positionB) * 0.5f + offset;
	const float radius = Vector3::Length(positionB - positionA) * 0.5f + frameMargin_;

	const Vector3 fromCenter = keyedPosition - outCenter;
	const float keyedDistance = Vector3::Length(fromCenter);
	if (keyedDistance < kMinAimDistance || radius <= 0.0f)
	{
		return false;
	}

	// 縦と横の画角の狭い方で収める。球が半画角 θ に収まる距離は r / sin(θ)（数式由来）
	const float halfFovY = fovY * 0.5f;
	const float halfFovX = std::atan(std::tan(halfFovY) * aspectRatio);
	const float halfFov = (std::min)(halfFovY, halfFovX);
	if (halfFov <= 0.0f)
	{
		return false;
	}
	outPosition = outCenter + fromCenter / keyedDistance * (radius / std::sin(halfFov));
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

	// 画角の土台は、キーがあればそれ、無ければ再生前に退避した画角（拍の揺れだけ付けたいとき用）。
	// カメラの今の画角を土台にすると、評価のたびに揺れが積み重なって純関数でなくなる
	float fov = 0.0f;
	bool hasFov = false;
	if (!fovCurve_.IsEmpty())
	{
		fov = fovCurve_.Evaluate(time);
		hasFov = true;
	}
	else if (hasCapturedState_ && !beatShakeCurve_.IsEmpty())
	{
		fov = capturedFov_;
		hasFov = true;
	}
	// 自動フレーミングの距離は、揺れを足す前の画角で決める。
	// このトラックが画角を決めないなら、退避した画角か今の画角（どちらもこのトラックは書き換えない）
	const float framingFov = hasFov ? fov : (hasCapturedState_ ? capturedFov_ : camera->GetFovY());

	// キーを持たないチャンネルには触れない。
	// 触れてしまうと「位置だけ演出する」トラックが画角を初期値に戻してしまう。
	// 原点が設定されていれば（カットシーンを現在地で再生する場合）、その位置と向きへ移す
	Vector3 framedCenter{};
	bool framed = false;
	if (!positionCurve_.IsEmpty())
	{
		const Vector3 keyedPosition = ctx.ApplyOriginToPoint(positionCurve_.Evaluate(time));
		Vector3 framedPosition{};
		framed = frameTargets_ && EvaluateFramedPosition(time, ctx, keyedPosition, framingFov, camera->GetAspectRatio(), framedPosition, framedCenter);
		camera->SetTranslate(framed ? framedPosition : keyedPosition);
	}

	// 向きは、注目点が決まればそちらを優先し、決まらなければ回転のキーを使う。
	// 自動フレーミング中は、役を包んだ球の中心を狙う
	Quaternion rotation = Quaternion::Identity();
	bool hasRotation = false;
	Vector3 aimTarget = framedCenter;
	const bool hasAim = framed || (UsesAim() && EvaluateAimTarget(time, ctx, aimTarget));
	if (hasAim)
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
		// 手持ちの揺れも同じ理由で、この評価で決めた向きにだけ重ねる
		if (!handheldCurve_.IsEmpty())
		{
			const float amplitude = handheldCurve_.Evaluate(time);
			const float pitch = amplitude * EvaluateHandheldAxis(kHandheldWaves[kHandheldPitchAxis], time, handheldFrequency_);
			const float yaw = amplitude * EvaluateHandheldAxis(kHandheldWaves[kHandheldYawAxis], time, handheldFrequency_);
			const float roll = amplitude * kHandheldRollScale * EvaluateHandheldAxis(kHandheldWaves[kHandheldRollAxis], time, handheldFrequency_);
			rotation = ApplyRoll(ApplyLocalTilt(rotation, pitch, yaw), roll);
		}
		camera->SetRotateQuaternion(rotation);
	}

	// ピントを注目点に合わせる。距離はカメラの位置と注目点だけから決まるので純関数のまま
	if (autoFocus_ && hasAim)
	{
		if (DepthOfFieldRenderer* depthOfField = ctx.GetDepthOfFieldRenderer())
		{
			DepthOfFieldRenderer::Settings& settings = depthOfField->GetSettings();
			settings.enabled = true;
			settings.focusDistance = Vector3::Length(aimTarget - camera->GetTranslate());
		}
	}

	if (hasFov)
	{
		const float kickDegrees = EvaluateBeatKick(time, ctx);
		fov = (std::max)(fov - kickDegrees * kDegreesToRadians, kMinFovDegrees * kDegreesToRadians);
		camera->SetFovY(fov);
	}
}

float CameraTrack::GetEndTime() const
{
	return (std::max)({ positionCurve_.GetEndTime(), rotationCurve_.GetEndTime(), fovCurve_.GetEndTime(),
		aimOffsetCurve_.GetEndTime(), aimBlendCurve_.GetEndTime(), rollCurve_.GetEndTime(),
		beatShakeCurve_.GetEndTime(), handheldCurve_.GetEndTime() });
}

void CameraTrack::CaptureState(const BindingContext& ctx)
{
	// 自動フォーカスを使うときだけ、被写界深度の設定を退避する。
	// 使わないのに戻すと、再生中に Settings で変えた値を消してしまう
	hasCapturedFocus_ = false;
	if (autoFocus_)
	{
		if (DepthOfFieldRenderer* depthOfField = ctx.GetDepthOfFieldRenderer())
		{
			const DepthOfFieldRenderer::Settings& settings = depthOfField->GetSettings();
			capturedFocusEnabled_ = settings.enabled;
			capturedFocusDistance_ = settings.focusDistance;
			hasCapturedFocus_ = true;
		}
	}

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
	if (hasCapturedFocus_)
	{
		if (DepthOfFieldRenderer* depthOfField = ctx.GetDepthOfFieldRenderer())
		{
			DepthOfFieldRenderer::Settings& settings = depthOfField->GetSettings();
			settings.enabled = capturedFocusEnabled_;
			settings.focusDistance = capturedFocusDistance_;
		}
	}

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

	// 注目点・混ぜ具合・ロール・揺れは今のカメラから読み取れないので、ここでは打たない
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
	json["beatShake"] = SerializeCurve(beatShakeCurve_);
	json["beatDivision"] = kBeatDivisionNames[static_cast<size_t>(beatDivision_)];
	json["beatShakeDecay"] = beatShakeDecay_;
	json["handheld"] = SerializeCurve(handheldCurve_);
	json["handheldFrequency"] = handheldFrequency_;
	json["autoFocus"] = autoFocus_;
	json["frameTargets"] = frameTargets_;
	json["frameMargin"] = frameMargin_;
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
	if (json.contains("beatShake"))
	{
		DeserializeCurve(json["beatShake"], beatShakeCurve_);
	}
	if (json.contains("beatDivision") && json["beatDivision"].is_string())
	{
		const std::string division = json["beatDivision"].get<std::string>();
		for (size_t i = 0; i < std::size(kBeatDivisionNames); ++i)
		{
			if (division == kBeatDivisionNames[i])
			{
				beatDivision_ = static_cast<BeatDivision>(i);
			}
		}
	}
	if (json.contains("beatShakeDecay") && json["beatShakeDecay"].is_number())
	{
		beatShakeDecay_ = std::clamp(json["beatShakeDecay"].get<float>(), kMinBeatShakeDecay, kMaxBeatShakeDecay);
	}
	if (json.contains("handheld"))
	{
		DeserializeCurve(json["handheld"], handheldCurve_);
	}
	if (json.contains("handheldFrequency") && json["handheldFrequency"].is_number())
	{
		handheldFrequency_ = std::clamp(json["handheldFrequency"].get<float>(), kMinHandheldFrequency, kMaxHandheldFrequency);
	}
	if (json.contains("autoFocus") && json["autoFocus"].is_boolean())
	{
		autoFocus_ = json["autoFocus"].get<bool>();
	}
	if (json.contains("frameTargets") && json["frameTargets"].is_boolean())
	{
		frameTargets_ = json["frameTargets"].get<bool>();
	}
	if (json.contains("frameMargin") && json["frameMargin"].is_number())
	{
		frameMargin_ = std::clamp(json["frameMargin"].get<float>(), 0.0f, kMaxFrameMargin);
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

	ImGui::SeparatorText("Focus / Framing");
	if (ImGui::Checkbox("Auto Focus On Aim", &autoFocus_))
	{
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("被写界深度をオンにして、ピントを注目点までの距離に合わせ続ける。\n再生を止めると元の設定に戻る");
	}
	if (ImGui::Checkbox("Frame Aim Roles", &frameTargets_))
	{
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Aim Role A / B が画面に収まる距離まで、カメラを自動で前後させる。\n向きは Position のキーから決め、画角はそのまま。Position のキーが要る");
	}
	if (frameTargets_)
	{
		if (ImGui::DragFloat("Frame Margin (m)", &frameMargin_, kFrameMarginDragSpeed, 0.0f, kMaxFrameMargin, "%.2f"))
		{
			frameMargin_ = std::clamp(frameMargin_, 0.0f, kMaxFrameMargin);
			changed = true;
		}
		if (positionCurve_.IsEmpty())
		{
			ImGui::TextDisabled("Position のキーが無いので、フレーミングは効かない");
		}
	}

	ImGui::SeparatorText("Shake");
	int division = static_cast<int>(beatDivision_);
	if (ImGui::Combo("Beat Division", &division, "Every Beat\0Every 2 Beats\0Every Bar\0"))
	{
		beatDivision_ = static_cast<BeatDivision>(division);
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Beat Shake のキーの強さ（度）だけ、この間隔ごとに画角がはねる。\nシーケンスの BPM が 0 だと揺れない");
	}
	if (ImGui::DragFloat("Beat Decay (s)", &beatShakeDecay_, kBeatShakeDecayDragSpeed, kMinBeatShakeDecay, kMaxBeatShakeDecay, "%.3f"))
	{
		beatShakeDecay_ = std::clamp(beatShakeDecay_, kMinBeatShakeDecay, kMaxBeatShakeDecay);
		changed = true;
	}
	if (ImGui::DragFloat("Handheld Speed (Hz)", &handheldFrequency_, kHandheldFrequencyDragSpeed, kMinHandheldFrequency, kMaxHandheldFrequency, "%.2f"))
	{
		handheldFrequency_ = std::clamp(handheldFrequency_, kMinHandheldFrequency, kMaxHandheldFrequency);
		changed = true;
	}
	if (fovCurve_.IsEmpty() && !beatShakeCurve_.IsEmpty())
	{
		ImGui::TextDisabled("FOV のキーが無いときは、再生前の画角を土台にしてはねる");
	}
	return changed;
}
#endif
} // namespace KCE
