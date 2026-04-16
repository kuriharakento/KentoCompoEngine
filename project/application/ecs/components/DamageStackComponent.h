#pragma once
#include <cstdint>

/**
 * @brief タレット「ダメージアップ」派生用。敵に付与されるダメージボーナススタック。
 *
 * - タレット or LMB ヒットでスタックが増加
 * - 次のヒット時にスタック分のボーナスダメージを適用して消費
 */
struct DamageStackComponent
{
	// 現在のスタック数
	int count_ = 0;

	// スタックあたりのダメージ増加量
	static constexpr float kDamagePerStack = 5.0f;

	// スタックの最大数
	static constexpr int kMaxStacks = 10;
};
