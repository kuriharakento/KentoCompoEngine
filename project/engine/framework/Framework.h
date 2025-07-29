#pragma once

// effects
#include "base/RenderTexture.h"
#include "manager/effect/PostProcessManager.h"
// scene
#include "engine/scene/factory/SceneFactory.h"
#include "engine/scene/manager/SceneManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/scene/LightManager.h"
// system
#include "base/DirectXCommon.h"
#include "base/WinApp.h"
#include "manager/system/SrvManager.h"
// editor
#include "manager/editor/ImGuiManager.h"
// graphics
#include "graphics/3d/Skybox.h"
#include "graphics/2d/SpriteCommon.h"
#include "graphics/3d/Object3dCommon.h"

class Framework
{
public: //メンバ関数
	//デストラクタ
	virtual ~Framework() = default;
	//初期化
	virtual void Initialize();
	//終了
	virtual void Finalize();
	//毎フレーム
	virtual void Update();
	//描画
	virtual void Draw() = 0;
	//3D描画用の設定
	void Draw3DSetting();
	//2D描画用の設定
	void Draw2DSetting();
	//パフォーマンス情報の表示
	void ShowPerformanceInfo();
	//終了リクエストがあるか
	virtual bool IsEndRequest() { return endRequest_; }
	//実行
	void Run();

protected: //メンバ変数
	//終了リクエスト
	bool endRequest_ = false;
	//ウィンドウアプリケーション
	std::unique_ptr<WinApp> winApp_;
	//DirectXCommon
	std::unique_ptr<DirectXCommon> dxCommon_;
	//SRVマネージャー
	std::unique_ptr<SrvManager> srvManager_;
	//ImGuiManager
	std::unique_ptr<ImGuiManager> imguiManager_;
	//スプライト共通部
	std::unique_ptr<SpriteCommon> spriteCommon_;
	//3Dオブジェクト共通部
	std::unique_ptr<Object3dCommon> objectCommon_;
	//カメラマネージャー
	std::unique_ptr<CameraManager> cameraManager_;
	//シーンマネージャー
	std::unique_ptr<SceneManager> sceneManager_;
	//シーンファクトリ
	std::unique_ptr<SceneFactory> sceneFactory_;
	//ライトマネージャー
	std::unique_ptr<LightManager> lightManager_;
	//
	std::unique_ptr<RenderTexture> renderTexture_;
	//
	std::unique_ptr<PostProcessManager> postProcessManager_;
	// Skybox
	std::unique_ptr<Skybox> skybox_;
};

