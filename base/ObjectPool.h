#pragma once

#include <vector>
#include <cstdint>

namespace KCE
{
/**
 * @brief メモリの動的確保を行わずにオブジェクトを高速再利用する汎用プール。
 *
 * - キャッシュ効率最大のパック連続メモリ
 * - HandleとGenerationによるダングリングポインタ防止
 * - 柔軟な枯渇ポリシー（フェイルセーフ対応）
 */
template<class T>
class ObjectPool
{
public:

	struct Handle
	{
		uint32_t index;
		uint32_t generation;

		static constexpr uint32_t kInvalid = 0xFFFFFFFF;

		bool IsValid() const
		{
			return index != kInvalid;
		}
	};

	enum class OverflowPolicy
	{
		Grow,		// 自動拡張（動的確保が発生するため毎フレーム非推奨）
		Fail,		// 取得失敗（安全な枯渇処理用）
		Recycle		// 最古のオブジェクトを上書き再利用（パーティクル等用）
	};

	struct Stats
	{
		uint32_t active;
		uint32_t free;
		uint32_t capacity;
	};

public:

	/**
	 * @brief プールを初期化し、指定容量だけメモリを事前確保する。
	 * @param capacity 最大オブジェクト数
	 * @param policy 枯渇時の挙動ポリシー
	 */
	void Initialize(uint32_t capacity, OverflowPolicy policy = OverflowPolicy::Fail);

	/**
	 * @brief オブジェクトを1つ要求し、ハンドルを返す。
	 * @return 取得したハンドル。Fail時に枯渇している場合は無効なHandleを返す。
	 */
	Handle Acquire();

	/**
	 * @brief 使用済みオブジェクトをプールに返却し、空きリストに戻す。
	 * @param handle 返却するハンドル
	 */
	void Release(Handle handle);

	/**
	 * @brief ハンドルから実際のオブジェクトポインタを取得する。
	 * @param handle 対象のハンドル
	 * @return 世代が一致していればメモリアドレス。無効や解放済みなら nullptr。
	 */
	T* Get(Handle handle);

	/**
	 * @brief プールの利用状況（アクティブ数・空き容量）を取得する。
	 */
	Stats GetStats() const;

	/**
	 * @brief アクティブな要素をすべて強制オフにして初期状態に戻す（メモリ解放はしない）。
	 */
	void Clear();

private:

	struct PoolItem
	{
		T payload;
		uint32_t generation = 0;
	};

	// 容量を拡張する（Growポリシー用）
	void ExpandCapacity(uint32_t newCapacity);

	// 最古の要素を強奪する（Recycleポリシー用）
	Handle RecycleOldest();

private:
	// 連続配置されるメモリ実体
	std::vector<PoolItem> items_;

	// 空きインデックスのスタック（O(1) 取得・返却用）
	std::vector<uint32_t> freeIndices_;
	
	// 稼働中インデックスの追跡用リングバッファ（Recycleポリシー用）
	std::vector<uint32_t> activeQueue_;
	uint32_t activeQueueHead_ = 0;
	uint32_t activeQueueTail_ = 0;

	uint32_t capacity_ = 0;
	OverflowPolicy policy_ = OverflowPolicy::Fail;
	uint32_t activeCount_ = 0;
};
} // namespace KCE
