#pragma once

class SrvManager;
class WinApp;
class DirectXCommon;
class ImGuiManager
{
public:
	/**
	 * \brief 初期化
	 * \param winApp 
	 * \param dxCommon 
	 * \param srvManager 
	 */
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * \brief 終了
	 */
	void Finalize();

	/**
	 * \brief ImGui受け付け開始
	 */
	void Begin();

	/**
	 * \brief 受付終了
	 */
	void End();

	/**
	 * \brief 画面への描画
	 */
	void Draw();

private:
	//ウィンドウアプリケーションのポインタ
	WinApp* winApp_ = nullptr;

	//SRVマネージャーのポインタ
	SrvManager* srvManager_ = nullptr;

	//DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;
};

