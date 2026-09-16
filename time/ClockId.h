#pragma once
#include <cstdint>

namespace KCE
{
struct TimeContext;

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

	// --- この時計の時間と操作。TimeManager を呼ぶだけ（例: owner->GetClock().GetDeltaTime()） ---

	/** @brief 今もある時計か */
	bool IsValid() const;
	/** @brief このフレームの時間。無効か指定なしなら Game の時間 */
	const TimeContext& GetContext() const;
	/** @brief 倍率を掛けた1フレームの経過時間。無効か指定なしなら Game の値 */
	float GetDeltaTime() const;
	/** @brief 倍率を掛けない1フレームの経過時間（止まっていれば 0）。無効か指定なしなら Game の値 */
	float GetRealDeltaTime() const;

	/**
	 * @brief 倍率を変える（1.0 が標準、0 で止まる）
	 * @details 操作の口は、無効か指定なしなら何もしない（間違えて Game を止めないように）。Real には効かない
	 */
	void SetTimeScale(float scale) const;
	/** @brief 止める。子も止まる。無効なら何もしない */
	void Pause() const;
	/** @brief 動かす。無効なら何もしない */
	void Resume() const;
	/** @brief この時計そのものが止められているか。無効なら偽 */
	bool IsPaused() const;
	/**
	 * @brief 指定した実時間だけ、この時計（と子）の更新時間を 0 にする。無効なら何もしない
	 * @param durationSeconds 秒。負数は 0 として扱う
	 */
	void StartHitStop(float durationSeconds) const;

	bool operator==(const ClockId& other) const { return index == other.index && generation == other.generation; }
	bool operator!=(const ClockId& other) const { return !(*this == other); }
};
} // namespace KCE
