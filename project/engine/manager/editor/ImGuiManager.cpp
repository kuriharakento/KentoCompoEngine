#include "ImGuiManager.h"

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

//system
#include "base/WinApp.h"
#include "manager/system/SrvManager.h"

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager)
{
#ifdef USE_IMGUI
	//引数をメンバ変数に記録
	winApp_ = winApp;
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	//ImGUiのコンテキストを生成
	ImGui::CreateContext();

	//ImGUiのスタイルを設定(好きに変えて大丈夫)
	ImGui::StyleColorsDark();

	//Win32用の初期化
	ImGui_ImplWin32_Init(winApp_->GetHwnd());

	//SRVの確保とインデックスの取得
	uint32_t srvIndex = srvManager_->Allocate();

	//DX12用の初期化
	ImGui_ImplDX12_Init(
		dxCommon_->GetDevice(),
		static_cast<int>(dxCommon_->GetBackBufferCount()),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		srvManager_->GetSrvHeap(),
		srvManager_->GetCPUDescriptorHandle(srvIndex),
		srvManager_->GetGPUDescriptorHandle(srvIndex)
	);

	// ドッキングを有効にする
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI
	//ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}

void ImGuiManager::Begin()
{
#ifdef USE_IMGUI
	//ImGuiの描画開始
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::End()
{
#ifdef USE_IMGUI
	//描画前準備
	ImGui::Render();
#endif
}

void ImGuiManager::Draw()
{
#ifdef USE_IMGUI
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	//ディスクリプタヒープの配列をセットするコマンド
	ID3D12DescriptorHeap* ppHeaps[] = { srvManager_->GetSrvHeap() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//描画コマンドを発行
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}
