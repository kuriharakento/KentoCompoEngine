#pragma once
#include <string>

namespace StringUtility
{
	//std::stringからstd::wstringへの変換
	std::wstring ConvertString(const std::string& str);
	//std::wstringからstd::stringへの変換
	std::string ConvertString(const std::wstring& wstr);
}
