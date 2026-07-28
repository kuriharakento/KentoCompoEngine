#pragma once

#include <filesystem>

namespace KCE
{
/**
 * @brief アプリケーションリソースのパス管理クラス
 */
class PathManager
{
public:
	/**
	 * @brief アプリケーションリソースルートを設定する
	 * @param path ルートディレクトリパス（例: "application/Resources"）
	 */
	static void SetApplicationResourceRoot(const std::filesystem::path& path);

	/**
	 * @brief アプリケーションリソースルートを取得する
	 * @return ルートディレクトリパス
	 */
	static const std::filesystem::path& GetApplicationResourceRoot();

	/**
	 * @brief 相対パスからアプリケーションリソースのフルパスを解決する
	 * @param relativePath 相対パス
	 * @return 解決されたパス
	 */
	static std::filesystem::path ResolveApplicationResource(const std::filesystem::path& relativePath);
};
} // namespace KCE
