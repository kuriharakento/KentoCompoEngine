#pragma once
#include <string>

/**
 * @brief 文字列変換ユーティリティを提供するネームスペース
 */
namespace StringUtility
{
	/**
	 * @brief std::stringからstd::wstringへの変換
	 * @param str 変換元のstd::string（UTF-8）
	 * @return 変換後のstd::wstring（UTF-16）
	 */
	std::wstring ConvertString(const std::string& str);

	/**
	 * @brief std::wstringからstd::stringへの変換
	 * @param wstr 変換元のstd::wstring（UTF-16）
	 * @return 変換後のstd::string（UTF-8）
	 */
	std::string ConvertString(const std::wstring& wstr);
}
