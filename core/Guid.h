#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace KCE
{
/**
 * @brief 128bitの安定ID
 * @details SEQUENCER_PLAN 3.3 の要求に対応する。演出データがオブジェクトを参照する際、
 *          名前で引くと改名・重複で壊れるため、参照は必ずこのGUIDで行い、名前は表示専用とする。
 *          文字列表現は32文字のハイフン無し小文字16進（例: "9f2c...":上位→下位の順）。
 */
struct Guid
{
	uint64_t high = 0;
	uint64_t low = 0;

	/**
	 * @brief 無効（ゼロ）なGUIDかどうか
	 * @return 上位・下位ともに0なら真
	 */
	bool IsValid() const { return high != 0 || low != 0; }

	bool operator==(const Guid& other) const { return high == other.high && low == other.low; }
	bool operator!=(const Guid& other) const { return !(*this == other); }
	bool operator<(const Guid& other) const
	{
		return high != other.high ? high < other.high : low < other.low;
	}

	/**
	 * @brief 新しいGUIDを生成する
	 * @return ランダムに生成されたGUID
	 */
	static Guid Generate();

	/**
	 * @brief 32文字の16進文字列に変換する
	 * @return 文字列表現
	 */
	std::string ToString() const;

	/**
	 * @brief 文字列からGUIDを復元する
	 * @param str 32文字の16進文字列（ハイフンは無視される）
	 * @param outGuid 復元先
	 * @return 解析に成功したら真。失敗時 outGuid は変更されない
	 */
	static bool TryParse(const std::string& str, Guid& outGuid);

	/**
	 * @brief 文字列からGUIDを復元する（失敗時は無効なGUID）
	 * @param str 32文字の16進文字列
	 * @return 復元されたGUID。解析失敗時は {0, 0}
	 */
	static Guid Parse(const std::string& str);
};
} // namespace KCE

namespace std
{
/** @brief GUIDを unordered_map のキーに使えるようにする */
template<>
struct hash<KCE::Guid>
{
	size_t operator()(const KCE::Guid& guid) const noexcept
	{
		// 上位と下位を黄金比定数で撹拌して結合する
		const size_t h1 = std::hash<uint64_t>{}(guid.high);
		const size_t h2 = std::hash<uint64_t>{}(guid.low);
		return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
	}
};
} // namespace std
