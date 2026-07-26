#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <memory>

// Logger::LogLevelの定義を使用するためインクルード
#include "base/Logger.h"

namespace KCE
{
#ifdef USE_IMGUI
/**
 * @brief 蓄積されたログエントリー情報
 */
struct LogEntry
{
	std::string message;
	Logger::LogLevel level;
};
#endif

/**
 * @brief コンソールログ管理クラス
 * @details デバッグログを蓄積し、ImGuiを用いたコンソール画面の描画を管理する
 */
class ConsoleLog
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return インスタンスへのポインタ
	 */
	static ConsoleLog* GetInstance();

	/**
	 * @brief インスタンスが有効かどうかを取得
	 * @return インスタンスが存在すれば真
	 */
	static bool HasInstance();

	/**
	 * @brief 初期化処理
	 */
	void Initialize();

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief ログをコンソールバッファに追加する
	 * @param message ログメッセージ
	 * @param level ログレベル
	 */
	void AddLog(const std::string& message, Logger::LogLevel level);

	/**
	 * @brief 蓄積されたログ履歴をクリアする
	 */
	void Clear();

	/**
	 * @brief コンソールウィンドウを描画する
	 * @param open ウィンドウの開閉状態フラグへのポインタ
	 */
	void Draw(bool* open);

private:
#ifdef USE_IMGUI
	// ログ履歴バッファ
	std::vector<LogEntry> logs_;
	// スレッド安全用のミューテックス
	std::mutex mutex_;
#endif

public:
	~ConsoleLog() = default;

private:
	static std::unique_ptr<ConsoleLog> instance_;

	ConsoleLog() = default;
	ConsoleLog(const ConsoleLog&) = delete;
	ConsoleLog& operator=(const ConsoleLog&) = delete;
};
} // namespace KCE
