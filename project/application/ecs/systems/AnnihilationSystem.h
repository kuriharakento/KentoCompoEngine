#include "engine/ecs/Entity.h"
#include "engine/ecs/system/ISystem.h"
#include <cstdint>

/**
 * @brief 連鎖爆発（アナイアレイション）を管理するシステム。
 * 
 * - ImpactChargeComponent がスタック最大に達した状態で死亡した際、
 *   周囲に誘爆ダメージとスタックを伝播させる。
 */
class AnnihilationSystem : public ISystem
{
public:
    void Update(Registry& registry) override;

private:
    // 爆発の実行
    void TriggerExplosion(EntityID sourceEntity, Registry& registry);
};
