#pragma once
#include "engine/ecs/system/ISystem.h"

/**
 * @brief スプリンクラーの自動起爆を管理するシステム。
 *
 * - SprinklerComponent を持つエンティティを毎フレーム処理
 * - 範囲内のボムスタック持ち敵を自動起爆する
 */
class SprinklerSystem : public ISystem
{
public:
	void Update(Registry& registry) override;
};
