#pragma once
#include <string>

namespace KCE
{
/**
 * @brief ゲームごとに決める起動時の設定。
 *
 * - ゲーム側は Framework::CreateGameConfig を上書きして返す
 * - 読むのは Framework::Initialize の頭で1回だけ。あとから書き換えても反映されない
 */
struct GameConfig
{
	// ウィンドウのタイトル
	std::wstring windowTitle = L"KentoCompo";
	// 最初に開くシーン（"TitleScene" の形。"Title" でもよい）
	std::string startSceneName = "TitleScene";
	// 起動したときにデバッグカメラをオンにしておくか（エディタのビルドだけ。F9 で切り替えられる）
	bool debugCameraEnabledOnStart = false;
};
} // namespace KCE
