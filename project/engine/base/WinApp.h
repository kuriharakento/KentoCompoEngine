#pragma once
#include <cstdint>
#include <Windows.h>

class WinApp
{
public: //静的メンバ関数
	//ウィンドウプロシージャ
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public: //メンバ関数
	//初期化
	void Initialize();
	//ウィンドウの終了
	void Finalize();

	//メッセージ処理
	bool ProcessMessage();

public: //アクセッサ
	//ウィンドウハンドルの取得
	HWND GetHwnd() const { return hwnd_; }
	//インスタンスハンドルの取得
	HINSTANCE GetHInstance() const { return wc_.hInstance; }

public: //定数
	//クライアント領域のサイズ
	static const int32_t kClientWidth = 1200;
	static const int32_t kClientHeight = 720;

private: //メンバ変数
	//ウィンドウハンドル
	HWND hwnd_ = nullptr;

	//ウィンドウクラスの設定
	WNDCLASS wc_{};

};


