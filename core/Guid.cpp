#include "core/Guid.h"

#include <random>

namespace KCE
{
namespace
{
/** @brief GUID生成用の乱数エンジン（プロセス内で共有） */
std::mt19937_64& GetRandomEngine()
{
	// 起動ごとに異なる系列にする。エディタが1セッションで生成する数には十分な品質。
	static std::mt19937_64 engine(std::random_device{}());
	return engine;
}

/** @brief 16進1文字を数値に変換する。16進でなければ -1 */
int HexCharToValue(char c)
{
	if (c >= '0' && c <= '9') { return c - '0'; }
	if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
	if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
	return -1;
}

/** @brief 64bit値を16文字の小文字16進にして追記する */
void AppendHex64(std::string& out, uint64_t value)
{
	static constexpr char kDigits[] = "0123456789abcdef";
	for (int shift = 60; shift >= 0; shift -= 4)
	{
		out.push_back(kDigits[(value >> shift) & 0xF]);
	}
}
} // namespace

Guid Guid::Generate()
{
	std::uniform_int_distribution<uint64_t> dist;
	Guid guid;
	guid.high = dist(GetRandomEngine());
	guid.low = dist(GetRandomEngine());
	// 万一すべて0になった場合、無効値と区別できなくなるので避ける
	if (!guid.IsValid()) { guid.low = 1; }
	return guid;
}

std::string Guid::ToString() const
{
	std::string result;
	result.reserve(32);
	AppendHex64(result, high);
	AppendHex64(result, low);
	return result;
}

bool Guid::TryParse(const std::string& str, Guid& outGuid)
{
	uint64_t values[2] = { 0, 0 };
	int digitCount = 0;

	for (char c : str)
	{
		// 表記ゆれを吸収するためハイフンは読み飛ばす
		if (c == '-') { continue; }

		const int value = HexCharToValue(c);
		if (value < 0) { return false; }
		if (digitCount >= 32) { return false; }

		values[digitCount / 16] = (values[digitCount / 16] << 4) | static_cast<uint64_t>(value);
		++digitCount;
	}

	if (digitCount != 32) { return false; }

	outGuid.high = values[0];
	outGuid.low = values[1];
	return true;
}

Guid Guid::Parse(const std::string& str)
{
	Guid guid;
	if (!TryParse(str, guid)) { return Guid{}; }
	return guid;
}
} // namespace KCE
