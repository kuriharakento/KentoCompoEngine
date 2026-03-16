#pragma once

#include "../../../engine/ecs/Registry.h"
#include "../components/LifetimeComponent.h"

/**
 * @brief エンティティの寿命を監視し、尽きた対象を遅延破棄キュー（Deferred Queue）に登録するシステム。
 * 
 * Updateフェーズの後半に実行し、メインループ内の「生き死にチェック」をこのシステム単一に集約する。
 */
class LifetimeSystem
{
public:
    /**
     * @brief レジストリ内の LifetimeComponent をすべて更新し、寿命切れを破棄予約する。
     * @param registry 対象のRegistry
     * @param deltaTime フレーム間の経過時間
     */
    static void Update(Registry& registry, float deltaTime);
};
