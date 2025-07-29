#include "base/WinApp.h"

#include "externals/imgui/imgui.h"
extern  IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib,"winmm.lib")

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
	{
		return true;
	}
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);

}

void WinApp::Initialize()
{
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	//システムタイマーの精度を上げる
	timeBeginPeriod(1);

	///===================================================================
	///ウィンドウを表示
	///===================================================================

	//ウィンドウプロシージャ
	wc_.lpfnWndProc = WindowProc;
	//ウィンドウクラス名
	wc_.lpszClassName = L"CG2WindowClass";
	//インスタンスハンドル
	wc_.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスを登録する
	RegisterClass(&wc_);

	//ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	//クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの設定
	hwnd_ = CreateWindow(
		wc_.lpszClassName,		//利用するクラス名
		L"myEngine",			//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,	//よく見るウィンドウスタイル
		CW_USEDEFAULT,			//表示X座標(WindowsOSに任せる)
		CW_USEDEFAULT,			//表示Y座標(WindowsOSに任せる)
		wrc.right - wrc.left,	//ウィンドウ横幅
		wrc.bottom - wrc.top,	//ウィンドウ縦幅
		nullptr,				//親ウィンドウハンドル
		nullptr,				//メニューハンドル
		wc_.hInstance,			//インスタンスハンドル
		nullptr					//オプション
	);

	//ウィンドウを表示する
	ShowWindow(hwnd_, SW_SHOW);

}

void WinApp::Finalize()
{
	//COMの終了処理
	CloseWindow(hwnd_);
	CoUninitialize();
}

bool WinApp::ProcessMessage()
{
	MSG msg;
	if(PeekMessage(&msg,nullptr,0,0,PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if(msg.message == WM_QUIT)
	{
		return true;
	}

	return false;
}
