#include "sequencer/core/BindingContext.h"

#include "math/MatrixFunc.h"

namespace KCE
{
namespace
{
/** @brief 未割り当ての役に対して返す空のライト名 */
const std::string kEmptyLightName;
} // namespace

void BindingContext::BindGameObject(const std::string& role, GameObject* gameObject)
{
	if (!gameObject)
	{
		gameObjects_.erase(role);
		return;
	}
	gameObjects_[role] = gameObject;
}

void BindingContext::BindCamera(const std::string& role, Camera* camera)
{
	if (!camera)
	{
		cameras_.erase(role);
		return;
	}
	cameras_[role] = camera;
}

void BindingContext::BindLight(const std::string& role, const std::string& lightName)
{
	if (lightName.empty())
	{
		lightNames_.erase(role);
		return;
	}
	lightNames_[role] = lightName;
}

GameObject* BindingContext::GetGameObject(const std::string& role) const
{
	const auto it = gameObjects_.find(role);
	return it != gameObjects_.end() ? it->second : nullptr;
}

Camera* BindingContext::GetCamera(const std::string& role) const
{
	const auto it = cameras_.find(role);
	return it != cameras_.end() ? it->second : nullptr;
}

const std::string& BindingContext::GetLightName(const std::string& role) const
{
	const auto it = lightNames_.find(role);
	return it != lightNames_.end() ? it->second : kEmptyLightName;
}

Vector3 BindingContext::ApplyOriginToPoint(const Vector3& localPosition) const
{
	if (!origin_.enabled)
	{
		return localPosition;
	}

	// 行ベクトル規約（v * M）で Y 軸回りに回してから平行移動する。
	// エンジンの MakeRotateYMatrix と同じ向きになるよう、行列から直接計算する
	const Matrix4x4 rotation = MakeRotateYMatrix(origin_.yaw);
	const Vector3 rotated = {
		localPosition.x * rotation.m[0][0] + localPosition.y * rotation.m[1][0] + localPosition.z * rotation.m[2][0],
		localPosition.x * rotation.m[0][1] + localPosition.y * rotation.m[1][1] + localPosition.z * rotation.m[2][1],
		localPosition.x * rotation.m[0][2] + localPosition.y * rotation.m[1][2] + localPosition.z * rotation.m[2][2],
	};
	return rotated + origin_.position;
}

Quaternion BindingContext::ApplyOriginToRotation(const Quaternion& localRotation) const
{
	if (!origin_.enabled)
	{
		return localRotation;
	}

	// クォータニオンの積の順序の規約に依存しないよう、行列で合成してから戻す。
	// 行ベクトル規約なので「ローカルの回転 → 原点の回転」の順に掛ける
	const Matrix4x4 combined = Multiply(localRotation.ToMatrix(), MakeRotateYMatrix(origin_.yaw));
	return Quaternion::FromMatrix(combined);
}

void BindingContext::Clear()
{
	gameObjects_.clear();
	cameras_.clear();
	lightNames_.clear();
}

bool BindingContext::IsEmpty() const
{
	return gameObjects_.empty() && cameras_.empty() && lightNames_.empty();
}
} // namespace KCE
