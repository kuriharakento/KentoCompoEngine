#include "Framework.h"

#include "audio/Audio.h"
#include "base/Logger.h"
#include "input/Input.h"

#include <Psapi.h>

// system
#include "graphics/2d/SpriteCommon.h"
#include "graphics/3d/Object3dCommon.h"
// manager
#include "manager/editor/JsonEditorManager.h"
#include "manager/graphics/TextureManager.h"
#include "manager/effect/ParticleManager.h"
#include "manager/graphics/ModelManager.h"
#include "manager/graphics/LineManager.h"
#include "time/TimeManager.h"
#include "time/TimerManager.h"

#ifdef USE_IMGUI
#include "ImGui/imgui_internal.h"
#endif

// ブラーレンダーターゲットの数（ピンポンバッファ用）
constexpr int kBlurRenderTargetCount = 2;
// レンダーテクスチャのクリアカラー
constexpr float kClearColorR = 0.1f;
constexpr float kClearColorG = 0.1f;
constexpr float kClearColorB = 0.1f;
constexpr float kClearColorA = 1.0f;
// デフォルトのカメラ位置
constexpr float kDefaultCameraY = 1.0f;
constexpr float kDefaultCameraZ = -10.0f;

void Framework::Initialize()
{
	/*----- システムの初期化（順序重要） -----*/

	// 1. ウィンドウアプリケーションの初期化
	winApp_ = std::make_unique<WinApp>();
	winApp_->Initialize();

	// 2. DirectXCommonの初期化
	dxCommon_ = std::make_unique<DirectXCommon>();
	dxCommon_->Initialize(winApp_.get());

	// 3. SRVマネージャーの初期化
	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Initialize(dxCommon_.get());

	// 4. ImGuiの初期化
	imguiManager_ = std::make_unique<ImGuiManager>();
	imguiManager_->Initialize(winApp_.get(), dxCommon_.get(), srvManager_.get());

	/*----- テクスチャ・グラフィックスの初期化 -----*/

	// テクスチャマネージャーの初期化
	TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

	// スプライト共通部の初期化
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get());

	// 3Dオブジェクト共通部の初期化
	objectCommon_ = std::make_unique<Object3dCommon>();
	objectCommon_->Initialize(dxCommon_.get(), srvManager_.get());

	// 3Dモデルマネージャーの初期化
	ModelManager::GetInstance()->Initialize(dxCommon_.get());

	// パーティクルマネージャーの初期化
	ParticleManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get());

	/*----- 入力・オーディオ・時間管理の初期化 -----*/

	// 入力の初期化
	Input::GetInstance()->Initialize(winApp_.get());

	// オーディオの初期化
	Audio::GetInstance()->Initialize();
	Audio::GetInstance()->SetDebugWindowVisible(true);

	// 時間管理クラスの初期化
	TimeManager::GetInstance();

	// タイマーマネージャーの初期化
	TimerManager::GetInstance();

	/*----- カメラ・シーン管理の初期化 -----*/

	// カメラマネージャーの初期化
	cameraManager_ = std::make_unique<CameraManager>();
	cameraManager_->AddCamera("main");
	cameraManager_->SetActiveCamera("main");
	cameraManager_->GetActiveCamera()->SetTranslate({ 0.0f, kDefaultCameraY, kDefaultCameraZ });
	cameraManager_->GetActiveCamera()->SetRotate({ 0.0f, 0.0f, 0.0f });

	// 3Dオブジェクト共通部に初期カメラをセット
	objectCommon_->SetDefaultCamera(cameraManager_->GetActiveCamera());

	// シーンファクトリーの初期化
	sceneFactory_ = std::make_unique<SceneFactory>();

	// シーンマネージャーの初期化
	sceneManager_ = std::make_unique<SceneManager>(sceneFactory_.get());

	/*----- ライト・ライン管理の初期化 -----*/

	// ライトマネージャーの初期化
	lightManager_ = std::make_unique<LightManager>();
	lightManager_->Initialize(dxCommon_.get());

	// ラインマネージャーの初期化
	LineManager::GetInstance()->Initialize(dxCommon_.get(), cameraManager_.get());

	/*----- レンダーテクスチャ・ポストプロセスの初期化 -----*/

	// メインレンダーテクスチャの初期化
	renderTexture_ = std::make_unique<RenderTexture>();
	Vector4 clearColor = { kClearColorR, kClearColorG, kClearColorB, kClearColorA };
	// DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: 標準的なsRGBフォーマット
	renderTexture_->Initialize(
		dxCommon_.get(),
		srvManager_.get(),
		WinApp::kClientWidth,
		WinApp::kClientHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		clearColor
	);

	// ブライトパス用レンダーターゲットの初期化
	brightPassRT_ = std::make_unique<RenderTexture>();
	// DXGI_FORMAT_R16G16B16A16_FLOAT: HDR処理用の浮動小数点フォーマット
	brightPassRT_->Initialize(
		dxCommon_.get(),
		srvManager_.get(),
		WinApp::kClientWidth,
		WinApp::kClientHeight,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		clearColor
	);

	// ブラー用レンダーターゲットの初期化（ピンポンバッファとして使用）
	for (int i = 0; i < kBlurRenderTargetCount; i++)
	{
		blurRT_[i] = std::make_unique<RenderTexture>();
		blurRT_[i]->Initialize(
			dxCommon_.get(),
			srvManager_.get(),
			WinApp::kClientWidth,
			WinApp::kClientHeight,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			clearColor
		);
	}

	// ポストプロセスマネージャーの初期化
	postProcessManager_ = std::make_unique<PostProcessManager>();
	postProcessManager_->Initialize(dxCommon_.get(), srvManager_.get(), L"Resources/shaders/PostEffect.VS.hlsl", L"Resources/shaders/PostEffect.PS.hlsl");
	postProcessManager_->SetBloomRenderTargets(
		brightPassRT_.get(),
		blurRT_[0].get(),
		blurRT_[1].get()
	);

	/*----- その他の初期化 -----*/

	// JSONエディターの初期化
	JsonEditorManager::GetInstance()->Initialize();

	// Skyboxの初期化
	skybox_ = std::make_unique<Skybox>();
}

void Framework::Finalize()
{
	/*----- 終了処理（初期化の逆順で解放） -----*/

	sceneManager_.reset();
	winApp_->Finalize();
	winApp_.reset();
	imguiManager_->Finalize();
	imguiManager_.reset();
	TextureManager::GetInstance()->Finalize();
	dxCommon_.reset();
	spriteCommon_.reset();
	objectCommon_.reset();
	ModelManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	Input::GetInstance()->Finalize();
	Audio::GetInstance()->Finalize();
	lightManager_.reset();
	LineManager::GetInstance()->Finalize();
	renderTexture_.reset();
	postProcessManager_.reset();
	brightPassRT_.reset();

	// ブラー用レンダーターゲットの解放
	for (int i = 0; i < kBlurRenderTargetCount; i++)
	{
		blurRT_[i].reset();
	}

	JsonEditorManager::GetInstance()->Finalize();
}

void Framework::Update()
{
	// ウィンドウメッセージ処理
	if (winApp_->ProcessMessage())
	{
		endRequest_ = true;
	}

	// ImGuiフレーム開始
	imguiManager_->Begin();

	// 入力の更新
	Input::GetInstance()->Update();

	// 時間計測の更新
	TimeManager::GetInstance().Update();

	// タイマーマネージャーの更新
	TimerManager::GetInstance().Update();

	// カメラの更新
	cameraManager_->Update();

	// オーディオの更新
	Audio::GetInstance()->Update();

	// ライトマネージャーの更新
	lightManager_->Update();

	// Skyboxの更新
	skybox_->Update(cameraManager_->GetActiveCamera());

	// JSONエディターの更新
	JsonEditorManager::GetInstance()->RenderEditUI();
}

void Framework::Draw3DSetting()
{
	// 3Dオブジェクト描画の共通設定
	objectCommon_->CommonRenderingSetting();

}

void Framework::Draw2DSetting()
{
	// スプライト描画の共通設定
	spriteCommon_->CommonRenderingSetting();
}

void Framework::Run()
{
	// 初期化
	Initialize();

	// メインループ開始ログ
	Logger::Log("\n/******* Start Main Loop *******/\n\n");

	// メインループ
	while (true)
	{
		// 更新
		Update();

		// 終了リクエストがあればループ終了
		if (IsEndRequest())
		{
			break;
		}

		// 描画
		Draw();
	}

	// メインループ終了ログ
	Logger::Log("\n/******* End Main Loop *******/\n\n");

	// 終了処理
	Finalize();
}

void Framework::ShowPerformanceInfo()
{
#ifdef USE_IMGUI
	// ウィンドウ位置を左上に固定
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	// ウィンドウサイズを固定
	ImGui::SetNextWindowSize(ImVec2(200, 65), ImGuiCond_Always);
	ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

	// FPS表示
	ImGui::Text("FPS : %.2f", ImGui::GetIO().Framerate);

	// メモリ使用量表示
	PROCESS_MEMORY_COUNTERS memInfo;
	GetProcessMemoryInfo(GetCurrentProcess(), &memInfo, sizeof(memInfo));
	ImGui::Text("Memory Usage : %.2f MB", memInfo.WorkingSetSize / (1024.0f * 1024.0f));

	ImGui::End();
#endif
}
