#pragma once
#include "engine/ecs/Entity.h"

/**
 * @brief デコイを管理するコンポーネント。
 */
struct DecoyComponent
{
	// 生成元のプレイヤー
	EntityID owner_ = kInvalidEntity;
};
