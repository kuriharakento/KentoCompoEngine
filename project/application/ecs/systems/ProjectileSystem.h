#pragma once
#include "engine/ecs/system/ISystem.h"
#include "engine/ecs/Entity.h"

/**
 * @brief 弾丸（Projectile）の移動、寿命、演出を管理するシステム。
 */
class ProjectileSystem : public ISystem
{
public:
    void Update(Registry& registry) override;

private:
    // 寿命が尽きた弾の削除
    void HandleExpiration(EntityID entity, Registry& registry);
};
