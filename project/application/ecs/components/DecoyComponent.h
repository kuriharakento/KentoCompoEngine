#pragma once
#include "engine/ecs/Entity.h"

/**
 * @brief デコイを管理するコンポーネント。
 */
struct DecoyComponent
{
	// 生成元のプレイヤー
	EntityID owner_ = kInvalidEntity;

	// 再生中のエフェクト（消滅時に停止させるため）
	class ParticleEffect* effect_ = nullptr;
};
