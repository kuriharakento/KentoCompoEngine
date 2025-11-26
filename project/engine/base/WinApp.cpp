#include "base/WinApp.h"

#include "externals/imgui/imgui.h"
extern  IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib,"winmm.lib")

// システムタイマー精度（ミリ秒）
constexpr UINT kTimerPrecision = 1;

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// ImGuiのメッセージ処理
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
	{
		return true;
	}
	switch (msg)
	{
	case WM_DESTROY:
		// ウィンドウ破棄時に終了メッセージを送信
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);

}

void WinApp::Initialize()
{
	// COMの初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// システムタイマーの精度を上げる
	timeBeginPeriod(kTimerPrecision);

	///===================================================================
	///ウィンドウを表示
	///===================================================================

	// ウィンドウプロシージャを設定
	wc_.lpfnWndProc = WindowProc;
	// ウィンドウクラス名を設定
	wc_.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドルを設定
	wc_.hInstance = GetModuleHandle(nullptr);
	// カーソルを設定
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録
	RegisterClass(&wc_);

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	// クライアント領域をもとに実際のサイズにwrcを変更
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		L"KentoCompo",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		nullptr
	);

	// ウィンドウを表示
	ShowWindow(hwnd_, SW_SHOW);

}

void WinApp::Finalize()
{
	// ウィンドウを閉じる
	CloseWindow(hwnd_);
	// COMの終了処理
	CoUninitialize();
}

bool WinApp::ProcessMessage()
{
	MSG msg;
	// メッセージがあれば処理
	if(PeekMessage(&msg,nullptr,0,0,PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// 終了メッセージを受け取った場合はtrueを返す
	if(msg.message == WM_QUIT)
	{
		return true;
	}

	return false;
}
