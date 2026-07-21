#include "SceneFactory.h"
#include "base/Logger.h"

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	auto& reg = GetRegistry();
	auto it = reg.find(sceneName);
	if (it != reg.end())
	{
		return it->second();
	}

	Logger::Log("[SceneFactory Error] Can't Create Scene: '" + sceneName + "' (Scene is not registered)\n");
	return nullptr;
}
