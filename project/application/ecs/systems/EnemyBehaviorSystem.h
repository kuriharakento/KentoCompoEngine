#pragma once

#include "../../../engine/ecs/Registry.h"
#include "../components/TransformComponent.h"
#include "../components/EnemyStateComponent.h"

/**
 * @brief 敵エンティティのAIと移動・ステートを更新するシステム。
 * 
 * Updateフェーズにて、入力や物理判定を考慮して
 * State や Transform を変更するためのロジックを実行する。
 */
class EnemyBehaviorSystem
{
public:
    /**
     * @brief レジストリ内の全対象コンポーネントを走査し、振る舞いを更新する。
     * @param registry 対象のRegistry
     * @param deltaTime フレーム間の経過時間
     */
    static void Update(Registry& registry, float deltaTime);
};
