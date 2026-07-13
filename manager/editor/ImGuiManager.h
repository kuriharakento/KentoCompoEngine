#pragma once

class SrvManager;
class WinApp;
class DirectXCommon;

/**
 * @brief ImGuiの管理クラス
 * @details ImGuiの初期化、更新、描画、終了処理を管理する
 *          ドッキング機能の有効化も行う
 */
class ImGuiManager
{
public:
	/**
	 * @brief 初期化
	 * @param winApp ウィンドウアプリケーションへのポインタ
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SRVマネージャーへのポインタ
	 */
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief 終了処理
	 * @details ImGuiのコンテキストを破棄し、リソースを解放する
	 */
	void Finalize();

	/**
	 * @brief ImGuiフレームの開始
	 * @details 新しいフレームの描画受付を開始する
	 */
	void Begin();

	/**
	 * @brief ImGuiフレームの終了
	 * @details 描画受付を終了し、レンダリング準備を行う
	 */
	void End();

	/**
	 * @brief 画面への描画
	 * @details ImGuiの描画コマンドを発行する
	 */
	void Draw();

private:
	// ウィンドウアプリケーションへのポインタ
	WinApp* winApp_ = nullptr;

	// SRVマネージャーへのポインタ
	SrvManager* srvManager_ = nullptr;

	// DirectXCommonへのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// FiraMono フォントのファイルパス
	const char* FiraMonoFontPath_ = "../engine/Resources/fonts/FiraMono-Medium.ttf";
};

