#include "GameOverScene.h"

// scene
#include "engine/scene/manager/SceneManager.h"
#include "manager/effect/PostProcessManager.h"
// math
#include "math/VectorColorCodes.h"
#include <input/Input.h>

void GameOverScene::Initialize()
{
	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		22, 16,
		1280.0f, 720.0f
	);

	// ブルームを無効化
	sceneManager_->GetPostProcessManager()->bloomEffect_->SetEnabled(false);

	// カメラの位置を設定
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate(Vector3());

	// スカイドームの初期化
	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(0.5f);
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });

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
		ChangeState(SceneState::Exit);
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
	titleFontSprite_->SetScale(0.5f);

	// リトライフォントスプライトの初期化
	retryFontSprite_ = std::make_unique<FontSprite>();
	retryFontSprite_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	retryFontSprite_->SetText("Retry");
	retryFontSprite_->SetPosition(kRetryFontSpritePosition);
	retryFontSprite_->SetScale(0.5f);

	// ゲームオーバーロゴ
	gameOverLogoFontSprite_ = std::make_unique<FontSprite>();
	gameOverLogoFontSprite_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	gameOverLogoFontSprite_->SetText("Game Over");
	gameOverLogoFontSprite_->SetPosition({ 250.0f, 200.0f });
	gameOverLogoFontSprite_->SetScale(1.0f);

	StartState(SceneState::Enter);
}

void GameOverScene::Finalize()
{
	sceneManager_->GetPostProcessManager()->bloomEffect_->SetEnabled(true);
}

void GameOverScene::Draw3D()
{
	skydome_->Draw();
}

void GameOverScene::Draw2D()
{
	// UIの描画
	gameOverToTitleUI_->Draw();
	gameOverRetryUI_->Draw();

	// フォントスプライトの描画
	titleFontSprite_->Draw();
	retryFontSprite_->Draw();
	// ゲームオーバーロゴの描画
	gameOverLogoFontSprite_->Draw();

	// シーン遷移エフェクトの描画
	transitionEffect_.Draw();
}

void GameOverScene::DrawImGui()
{
}

// ==================================================
// Enter状態（シーン開始・フェードイン演出）
// ==================================================
void GameOverScene::OnEnterEnter()
{
	// 赤から黒へのフェードイン（ゲームオーバーの雰囲気を演出）
	transitionEffect_.SetFadeType(FadeType::FadeOut);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetMode(TransitionMode::CenterToEdges);
	transitionEffect_.Start(
		1.0f,
		VectorColorCodes::Red,
		VectorColorCodes::Black
	);
}

void GameOverScene::OnUpdateEnter()
{
	transitionEffect_.Update();

	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		ChangeState(SceneState::Playing);
	}
}

void GameOverScene::OnExitEnter()
{
}

// ==================================================
// Playing状態（ゲームオーバー表示・入力待ち）
// ==================================================
void GameOverScene::OnEnterPlaying()
{
}

void GameOverScene::OnUpdatePlaying()
{
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::EdgesToCenter);
		transitionEffect_.Start(
			1.0f,
			VectorColorCodes::Black,
			VectorColorCodes::Red
		);
		ChangeState(SceneState::Exit);
	}
}

void GameOverScene::OnExitPlaying()
{
}

// ==================================================
// Exit状態（シーン退場・タイトルへ遷移）
// ==================================================
void GameOverScene::OnEnterExit()
{
	transitionEffect_.SetFadeType(FadeType::FadeIn);
	transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
	transitionEffect_.SetMode(TransitionMode::EdgesToCenter);
	transitionEffect_.Start(
		1.0f,
		VectorColorCodes::Red,
		VectorColorCodes::Black
	);
}

void GameOverScene::OnUpdateExit()
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

void GameOverScene::OnExitExit()
{
}

void GameOverScene::CommonUpdate()
{
	skydome_->Update(sceneManager_->GetCameraManager());
	gameOverToTitleUI_->Update();
	gameOverRetryUI_->Update();
	titleFontSprite_->Update();
	retryFontSprite_->Update();
	gameOverLogoFontSprite_->Update();
}