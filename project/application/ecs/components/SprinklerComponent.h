#pragma once
#include <cstdint>
#include "engine/ecs/Entity.h"

/**
 * @brief スプリンクラーの状態を管理するコンポーネント。
 *
 * - Qスキル「スプリンクラー」派生で地面に設置される。
 * - 範囲内のボムスタック持ち敵を自動起爆する。
 */
struct SprinklerComponent
{
	// 所有プレイヤー（所有しない。プレイヤーのほうが長寿命の前提）
	EntityID owner_ = kInvalidEntity;

	// 起爆チェック範囲
	float range_ = 15.0f;

	// 起爆チェック間隔（秒）
	float checkInterval_ = 0.5f;
	float checkTimer_ = 0.0f;

	// 残り時間
	float lifetime_ = 8.0f;
};
