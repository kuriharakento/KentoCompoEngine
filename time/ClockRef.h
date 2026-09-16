#pragma once
#include <string_view>

#include "time/ClockId.h"

namespace KCE
{
/**
 * @brief 名前で時計を指す軽い参照
 *
 * - 初めて使うときだけ名前で探し、あとは覚えた ClockId を使う（毎回名前で探す無駄が無い）
 * - 時計が消されたり作り直されたりしたら、次に使うときに自動で探し直す
 * - 見つからない間は、読む口では Game の時間になり、操作の口では何もしない（警告は名前ごとに1回）
 * - ClockId を受け取る口にそのまま渡せる（例: time.GetDeltaTime(kEnemyClock)、object->SetClock(kEnemyClock)）
 * - 名前は文字列リテラルを渡す想定。string_view で持つので、渡した文字列はこれより長く生きること
 * - メインスレッドから使う
 *
 * 例: static KCE::ClockRef kEnemyClock{"Enemy"};
 */
class ClockRef
{
public:
	/**
	 * @brief 名前だけ覚える。ここでは探さない（起動時の CreateClock より前に作ってよい）
	 * @param name 時計の名前（文字列リテラルなど、これより長く生きるもの）
	 */
	constexpr explicit ClockRef(std::string_view name) : name_(name) {}

	/**
	 * @brief 時計の番号。必要なら名前で探し直す
	 * @return 見つからなければ指定なしの ClockId
	 */
	ClockId Get() const;

	/** @brief ClockId を受け取る口にそのまま渡すため */
	operator ClockId() const { return Get(); }

	// --- ClockId と同じ口（例: kEnemyClock.GetDeltaTime()）。見つからない間は読む口は Game、操作の口は何もしない ---

	/** @brief このフレームの時間 */
	const TimeContext& GetContext() const { return Get().GetContext(); }
	/** @brief 倍率を掛けた1フレームの経過時間 */
	float GetDeltaTime() const { return Get().GetDeltaTime(); }
	/** @brief 倍率を掛けない1フレームの経過時間（止まっていれば 0） */
	float GetRealDeltaTime() const { return Get().GetRealDeltaTime(); }
	/** @brief 倍率を変える */
	void SetTimeScale(float scale) const { Get().SetTimeScale(scale); }
	/** @brief 止める。子も止まる */
	void Pause() const { Get().Pause(); }
	/** @brief 動かす */
	void Resume() const { Get().Resume(); }
	/** @brief この時計そのものが止められているか */
	bool IsPaused() const { return Get().IsPaused(); }
	/** @brief ヒットストップする */
	void StartHitStop(float durationSeconds) const { Get().StartHitStop(durationSeconds); }

	/** @brief 時計の名前 */
	constexpr std::string_view GetName() const { return name_; }

private:
	std::string_view name_;
	// 最後に見つけた時計の番号。消されたら Get で探し直す
	mutable ClockId cached_{};
};
} // namespace KCE
