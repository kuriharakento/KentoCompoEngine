#include "SystemManager.h"

void SystemManager::AddSystem(std::shared_ptr<ISystem> system)
{
	assert(system != nullptr);
	systems_.push_back(system);
	
	// 型情報での逆引き用登録（先着優先、同じ型を複数登録する場合は GetSystem では最初のものが返る）
	std::type_index typeIndex = std::type_index(typeid(*system));
	if (systemMap_.find(typeIndex) == systemMap_.end())
	{
		systemMap_[typeIndex] = system;
	}
}

void SystemManager::Initialize()
{
	for (auto& system : systems_)
	{
		system->Initialize();
	}
}

void SystemManager::Update(Registry& registry)
{
	for (auto& system : systems_)
	{
		system->Update(registry);
	}
}

void SystemManager::Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
	for (auto& system : systems_)
	{
		system->Draw(registry, camera, lightManager, shadowMapManager);
	}
}

void SystemManager::DrawShadow(Registry& registry)
{
	for (auto& system : systems_)
	{
		system->DrawShadow(registry);
	}
}

void SystemManager::Finalize()
{
	for (auto& system : systems_)
	{
		system->Finalize();
	}
	systems_.clear();
	systemMap_.clear();
}
