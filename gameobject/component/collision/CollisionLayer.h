#pragma once
#include <cstdint>

namespace GameObjectComponent
{
	/**
	 * @brief 衝突判定用のレイヤー型（32ビットのビットマスク）
	 */
	using ColliderLayer = uint32_t;

	namespace ColliderLayers
	{
		static constexpr ColliderLayer None = 0;
		static constexpr ColliderLayer All  = 0xFFFFFFFF;
	}
}
