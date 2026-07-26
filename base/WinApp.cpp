#include "base/WinApp.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"

extern  IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#pragma comment(lib,"winmm.lib")

namespace KCE
{
// システムタイマー精度（ミリ秒）
constexpr UINT kTimerPrecision = 1;

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// ImGuiのメッセージ処理
#ifdef USE_IMGUI
	if (::ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
	{
		return true;
	}
#endif

	WinApp* winApp = reinterpret_cast<WinApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (msg)
	{
	case WM_CREATE:
		{
			LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
			winApp = reinterpret_cast<WinApp*>(createStruct->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(winApp));
		}
		return 0;

	case WM_SIZE:
		if (winApp && wparam != SIZE_MINIMIZED)
		{
			uint32_t width = LOWORD(lparam);
			uint32_t height = HIWORD(lparam);
			winApp->width_ = width;
			winApp->height_ = height;
			if (winApp->resizeCallback_)
			{
				winApp->resizeCallback_(width, height);
			}
		}
		return 0;

	case WM_DESTROY:
		// ウィンドウ破棄時に終了メッセージを送信
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WinApp::Initialize()
{
	// DPI Awareness の設定 (Per-Monitor DPI Aware V2)
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// COMの初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	// システムタイマーの精度を上げる
	timeBeginPeriod(kTimerPrecision);

	/** @brief ウィンドウを表示 */

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
	RECT wrc = { 0,0,static_cast<LONG>(width_),static_cast<LONG>(height_) };

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
		this
	);

	// ウィンドウを最大化表示（ボーダー付きフルスクリーン）
	ShowWindow(hwnd_, SW_SHOWMAXIMIZED);

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

void WinApp::Resize(uint32_t width, uint32_t height)
{
	width_ = width;
	height_ = height;

	RECT rect = { 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

	SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}
} // namespace KCE
