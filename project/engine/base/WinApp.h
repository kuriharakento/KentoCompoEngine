#pragma once
#include <cstdint>
#include <Windows.h>

/**
 * @brief Windowsアプリケーション管理クラス
 */
class WinApp
{
public:
	/**
	 * @brief ウィンドウプロシージャ
	 * @param hwnd ウィンドウハンドル
	 * @param msg メッセージ
	 * @param wparam wパラメータ
	 * @param lparam lパラメータ
	 * @return 処理結果
	 */
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
	/**
	 * @brief 初期化
	 */
	void Initialize();

	/**
	 * @brief ウィンドウの終了処理
	 */
	void Finalize();

	/**
	 * @brief メッセージ処理
	 * @return 終了メッセージを受け取った場合はtrue
	 */
	bool ProcessMessage();

public:
	/**
	 * @brief ウィンドウハンドルを取得
	 * @return ウィンドウハンドル
	 */
	HWND GetHwnd() const { return hwnd_; }

	/**
	 * @brief インスタンスハンドルを取得
	 * @return インスタンスハンドル
	 */
	HINSTANCE GetHInstance() const { return wc_.hInstance; }

public:
	// クライアント領域の幅
	static const int32_t kClientWidth = 1280;
	// クライアント領域の高さ
	static const int32_t kClientHeight = 720;

private:
	// ウィンドウハンドル
	HWND hwnd_ = nullptr;
	// ウィンドウクラス
	WNDCLASS wc_{};

};


