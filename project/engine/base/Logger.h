#pragma once
#include <string>

/**
 * @brief ログ出力機能を提供するネームスペース
 */
namespace Logger
{
	/**
	 * @brief デバッグ出力にメッセージを出力
	 * @param message 出力するメッセージ（UTF-8）
	 */
	void Log(const std::string& message);
}
