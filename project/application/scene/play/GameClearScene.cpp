#include "GameClearScene.h"

// scene
#include "engine/scene/manager/SceneManager.h"
#include "manager/effect/PostProcessManager.h"
// math
#include "math/VectorColorCodes.h"
#include <input/Input.h>
#include <audio/Audio.h>

void GameClearScene::Initialize()
{
	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		kTransitionGridX, kTransitionGridY,
		WinApp::kClientWidth, WinApp::kClientHeight
	);

	// ブルームを無効化
	sceneManager_->GetPostProcessManager()->bloomEffect_->SetEnabled(false);

	// カメラの初期設定
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(Vector3());
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate(kInitialCameraDirection);

	// ゲームオーバーからタイトルUIの初期化
	gameOverToTitleUI_ = std::make_unique<GameUI>();
	gameOverToTitleUI_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/black.png");
	gameOverToTitleUI_->SetScreenPosition(kGameOverToTitleUIPosition);
	gameOverToTitleUI_->SetSize(kGameOverUISize);
	gameOverToTitleUI_->SetAnchorPoint(kGameOverUIAnchorPoint);
	gameOverToTitleUI_->SetColor(VectorColorCodes::Black);
	// コールバックの設定
	gameOverToTitleUI_->SetInteractable(true);
	gameOverToTitleUI_->SetOnClickCallback([this]() {
		returnToTitle_ = true;
		Audio::GetInstance()->PlayWave("start_se", false);
		ChangeState(SceneState::Exit);
		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::EdgesToCenter);
		transitionEffect_.Start(
			kTransitionDuration,
			VectorColorCodes::Black,
			VectorColorCodes::Green
		);
		gameOverToTitleUI_->SetInteractable(false);
										   });
	gameOverToTitleUI_->SetOnHoverStayCallback([this]() {
		gameOverToTitleUI_->SetColor(VectorColorCodes::White);
		titleFontSprite_->SetColor(VectorColorCodes::Black);
											   });
	gameOverToTitleUI_->SetOnHoverExitCallback([this]() {
		gameOverToTitleUI_->SetColor(VectorColorCodes::Black);
		titleFontSprite_->SetColor(VectorColorCodes::White);
											   });

	// ゲームオーバーからリトライUIの初期化
	gameOverRetryUI_ = std::make_unique<GameUI>();
	gameOverRetryUI_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/black.png");
	gameOverRetryUI_->SetScreenPosition(kGameOverRetryUIPosition);
	gameOverRetryUI_->SetSize(kGameOverUISize);
	gameOverRetryUI_->SetAnchorPoint(kGameOverUIAnchorPoint);
	gameOverRetryUI_->SetColor(VectorColorCodes::Black);
	// コールバックの設定
	gameOverRetryUI_->SetInteractable(true);
	gameOverRetryUI_->SetOnClickCallback([this]() {
		retry_ = true;
		Audio::GetInstance()->PlayWave("start_se", false);
		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::EdgesToCenter);
		transitionEffect_.Start(
			kTransitionDuration,
			VectorColorCodes::Black,
			VectorColorCodes::Green
		);
		ChangeState(SceneState::Exit);
		gameOverRetryUI_->SetInteractable(false);
										 });
	gameOverRetryUI_->SetOnHoverStayCallback([this]() {
		gameOverRetryUI_->SetColor(VectorColorCodes::White);
		retryFontSprite_->SetColor(VectorColorCodes::Black);
											 });
	gameOverRetryUI_->SetOnHoverExitCallback([this]() {
		gameOverRetryUI_->SetColor(VectorColorCodes::Black);
		retryFontSprite_->SetColor(VectorColorCodes::White);
											 });

	// タイトルフォントスプライトの初期化
	titleFontSprite_ = std::make_unique<FontSprite>();
	titleFontSprite_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	titleFontSprite_->SetText("Title");
	titleFontSprite_->SetPosition(kTitleFontSpritePosition);
	titleFontSprite_->SetScale(kButtonFontScale);

	// リトライフォントスプライトの初期化
	retryFontSprite_ = std::make_unique<FontSprite>();
	retryFontSprite_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	retryFontSprite_->SetText("Retry");
	retryFontSprite_->SetPosition(kRetryFontSpritePosition);
	retryFontSprite_->SetScale(kButtonFontScale);

	// ゲームオーバーロゴ
	gameClearLogoFontSprite_ = std::make_unique<FontSprite>();
	gameClearLogoFontSprite_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	gameClearLogoFontSprite_->SetText("Game Clear");
	gameClearLogoFontSprite_->SetPosition(kGameClearLogoPosition);
	gameClearLogoFontSprite_->SetScale(kLogoFontScale);

	StartState(SceneState::Enter);
}

void GameClearScene::Finalize()
{
	ClearObjects();
	sceneManager_->GetPostProcessManager()->bloomEffect_->SetEnabled(true);
}

void GameClearScene::Draw3D()
{
	BaseScene::Draw3D();
}

void GameClearScene::DrawShadow()
{
}

void GameClearScene::DrawGBuffer()
{
}

void GameClearScene::Draw2D()
{
	// UIの描画
	gameOverToTitleUI_->Draw();
	gameOverRetryUI_->Draw();

	// フォントスプライトの描画
	titleFontSprite_->Draw();
	retryFontSprite_->Draw();
	// ゲームオーバーロゴの描画
	gameClearLogoFontSprite_->Draw();

	// シーン遷移エフェクトの描画
	transitionEffect_.Draw();
}

void GameClearScene::DrawImGui()
{
}

// ==================================================
// Enter状態（シーン開始・フェードイン演出）
// ==================================================
void GameClearScene::OnEnterEnter()
{
	// 赤から黒へのフェードイン（ゲームオーバーの雰囲気を演出）
	transitionEffect_.SetFadeType(FadeType::FadeOut);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetMode(TransitionMode::CenterToEdges);
	transitionEffect_.Start(
		kTransitionDuration,
		VectorColorCodes::Green,
		VectorColorCodes::Black
	);
}

void GameClearScene::OnUpdateEnter()
{
	transitionEffect_.Update();

	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		ChangeState(SceneState::Playing);
	}
}

void GameClearScene::OnExitEnter()
{
}

// ==================================================
// Playing状態（ゲームオーバー表示・入力待ち）
// ==================================================
void GameClearScene::OnEnterPlaying()
{
}

void GameClearScene::OnUpdatePlaying()
{
}

void GameClearScene::OnExitPlaying()
{
}

// ==================================================
// Exit状態（シーン退場・タイトルへ遷移）
// ==================================================
void GameClearScene::OnEnterExit()
{
	transitionEffect_.SetFadeType(FadeType::FadeIn);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetMode(TransitionMode::EdgesToCenter);
	transitionEffect_.Start(
		kTransitionDuration,
		VectorColorCodes::Green,
		VectorColorCodes::Black
	);
}

void GameClearScene::OnUpdateExit()
{
	transitionEffect_.Update();

	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		// タイトルへ戻る場合
		if (returnToTitle_)
		{
			sceneManager_->ChangeScene(SceneNames::Title);
		}
		// リトライする場合
		else if (retry_)
		{
			sceneManager_->ChangeScene(SceneNames::GamePlay);
		}
	}
}

void GameClearScene::OnExitExit()
{
}

void GameClearScene::CommonUpdate()
{
	gameOverToTitleUI_->Update();
	gameOverRetryUI_->Update();
	titleFontSprite_->Update();
	retryFontSprite_->Update();
	gameClearLogoFontSprite_->Update();
}