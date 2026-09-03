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
			if (width == 0 || height == 0 || (width == winApp->width_ && height == winApp->height_)) return 0;
			winApp->width_ = width;
			winApp->height_ = height;
			if (winApp->resizeCallback_)
			{
				winApp->resizeCallback_(width, height);
			}
		}
		return 0;

	case WM_ERASEBKGND:
		// D3D12 owns every presented pixel. Letting DefWindowProc erase the client
		// area can expose a blank frame while the GPU is under heavy particle load.
		return 1;

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

	// 通常起動は生成時点から最大化状態にする。性能試験ではモニターの
	// taskbar/work-areaに左右されず、正確な1920x1080 clientを使用する。
	char fixed1080p[8] = {};
	const DWORD fixed1080pLength = GetEnvironmentVariableA(
		"KCE_FIXED_1080P", fixed1080p, static_cast<DWORD>(sizeof(fixed1080p)));
	const bool useFixed1080p = fixed1080pLength > 0 && fixed1080pLength < sizeof(fixed1080p) &&
		!(fixed1080pLength == 1 && fixed1080p[0] == '0');
	const DWORD windowStyle = WS_OVERLAPPEDWINDOW | (useFixed1080p ? 0 : WS_MAXIMIZE);

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,static_cast<LONG>(width_),static_cast<LONG>(height_) };

	// クライアント領域をもとに実際のサイズにwrcを変更
	AdjustWindowRect(&wrc, windowStyle, false);

	// ウィンドウの生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		L"KentoCompo",
		windowStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		this
	);

}

void WinApp::Show()
{
	if (hwnd_ == nullptr || IsWindowVisible(hwnd_))
	{
		return;
	}

	// GPUリソースとシーンの初期化完了後に初めて表示する。
	// CreateWindow直後に表示すると、初回Presentまで未描画のクライアント領域が
	// 一瞬露出し、画面全体が消えたように見えるため遅延させる。
	ShowWindow(hwnd_, SW_SHOW);
	UpdateWindow(hwnd_);
}

void WinApp::SetWindowTitle(const std::wstring& title)
{
	SetWindowText(hwnd_, title.c_str());
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
	MSG msg{};
	// Drain the queue every frame. Processing only one message per frame lets
	// paint/resize/input messages accumulate when a heavy GPU workload lowers
	// the frame rate, which can make the compositor temporarily show no client
	// content and makes the application appear to flicker.
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT) return true;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
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
