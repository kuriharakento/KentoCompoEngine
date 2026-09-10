#include "sequencer/core/BindingContext.h"

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
