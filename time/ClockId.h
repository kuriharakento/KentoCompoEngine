#pragma once
#include <cstdint>

namespace KCE
{
/**
 * @brief TimeManager の時計を指す番号
 *
 * - 時計そのものは TimeManager が持つ。使う側（GameObject・パーティクル・タイマーなど）はこの番号だけを覚える
 * - 消した時計の番号は世代がずれて無効になる。無効な番号は Game の時計として扱われるので、壊れたポインタにならない
 * - 既定値は「指定なし」。GameObject は親の時計、それも無ければ Game を使う
 */
struct ClockId
{
	// 指定なしを表す番号
	static constexpr uint32_t kInvalidIndex = UINT32_MAX;

	// TimeManager の時計の配列の番号
	uint32_t index = kInvalidIndex;
	// 同じ番号の時計を作り直したときに見分けるための世代
	uint32_t generation = 0;

	/** @brief 時計が指定されているか（消えていても真。消えたかは TimeManager::IsValid で見る） */
	bool IsSpecified() const { return index != kInvalidIndex; }

	bool operator==(const ClockId& other) const { return index == other.index && generation == other.generation; }
	bool operator!=(const ClockId& other) const { return !(*this == other); }
};
} // namespace KCE
