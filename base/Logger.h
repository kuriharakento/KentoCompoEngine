#pragma once
#include <string>

namespace KCE
{

/**
 * @brief ログ出力機能を提供するネームスペース
 */
namespace Logger
{
	/**
	 * @brief ログの重要度レベル
	 */
	enum class LogLevel
	{
		Info,
		Warning,
		Error
	};

	/**
	 * @brief デバッグ出力にメッセージを出力
	 * @param message 出力するメッセージ（UTF-8）
	 * @param level 重要度レベル
	 */
	void Log(const std::string& message, LogLevel level = LogLevel::Info);
}

} // namespace KCE

