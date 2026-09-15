#pragma once
#include <cstdint>

namespace KCE
{
/**
 * @brief ドローコールを数える
 *
 * - 描画コマンド（DrawIndexedInstanced など）を積んだ直後に Add() を呼ぶ
 * - RenderProfiler がパスの前後で Get() の差を取って、パスごとの回数にする
 * - 描画はメインスレッドだけで積むので、排他はしない
 */
class DrawCallCounter
{
public:
	/** @brief ドローコールを1回数える */
	static void Add() { ++count_; }

	/**
	 * @brief 起動してから数えた回数
	 * @return 戻さずに増え続ける。差を取って使う
	 */
	static uint64_t Get() { return count_; }

private:
	inline static uint64_t count_ = 0;
};
} // namespace KCE
