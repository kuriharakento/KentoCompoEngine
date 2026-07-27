#pragma once
#include <cstdint>
#include <Windows.h>
#include <functional>
#include <string>

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
	 * @brief ウィンドウタイトルを変更する。
	 * @param title ウィンドウタイトル
	 */
	void SetWindowTitle(const std::wstring& title);

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
	static const int32_t kClientWidth = 1920;
	// クライアント領域の高さ
	static const int32_t kClientHeight = 1080;

public:
	/**
	 * @brief ウィンドウサイズを変更する。
	 * @param width クライアント領域の幅
	 * @param height クライアント領域の高さ
	 */
	void Resize(uint32_t width, uint32_t height);

	/**
	 * @brief リサイズ時のコールバックを登録する。
	 * @param callback コールバック関数
	 */
	void SetResizeCallback(std::function<void(uint32_t, uint32_t)> callback) { resizeCallback_ = callback; }

	/**
	 * @brief クライアント領域の幅を取得する。
	 */
	uint32_t GetClientWidth() const { return width_; }

	/**
	 * @brief クライアント領域の高さを取得する。
	 */
	uint32_t GetClientHeight() const { return height_; }

private:
	// ウィンドウハンドル
	HWND hwnd_ = nullptr;
	// ウィンドウクラス
	WNDCLASS wc_{};
	// クライアント領域の幅
	uint32_t width_ = kClientWidth;
	// クライアント領域の高さ
	uint32_t height_ = kClientHeight;
	// リサイズ時のコールバック
	std::function<void(uint32_t, uint32_t)> resizeCallback_;

};


