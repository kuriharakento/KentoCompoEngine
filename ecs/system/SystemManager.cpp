#include "SystemManager.h"

namespace KCE
{
void SystemManager::AddSystem(std::unique_ptr<ISystem> system)
{
    assert(system != nullptr);
    
    // 型情報の取得
    std::type_index typeIndex = std::type_index(typeid(*system));
    
    // マップへの登録 (非所有)
    if (systemMap_.find(typeIndex) == systemMap_.end())
    {
        systemMap_[typeIndex] = system.get();
    }
    
    // リストへの登録 (所有権移転)
    systems_.push_back(std::move(system));
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
} // namespace KCE
