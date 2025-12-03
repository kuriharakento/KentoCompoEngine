#include "GameOverScene.h"

// scene
#include "engine/scene/manager/SceneManager.h"
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

	// ゲームオーバーからタイトルUIの初期化
	gameOverToTitleUI_ = std::make_unique<GameUI>();
	gameOverToTitleUI_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/black.png");
	gameOverToTitleUI_->SetScreenPosition(kGameOverToTitleUIPosition);
	gameOverToTitleUI_->SetSize(kGameOverUISize);
	gameOverToTitleUI_->SetAnchorPoint(kGameOverUIAnchorPoint);
	// コールバックの設定
	gameOverToTitleUI_->SetInteractable(true);
	gameOverToTitleUI_->SetOnClickCallback([this]() {
		returnToTitle_ = true;
		ChangeState(SceneState::Exit);
		gameOverToTitleUI_->SetInteractable(false);
										   });

	// ゲームオーバーからリトライUIの初期化
	gameOverRetryUI_ = std::make_unique<GameUI>();
	gameOverRetryUI_->Initialize(sceneManager_->GetSpriteCommon(), "./Resources/black.png");
	gameOverRetryUI_->SetScreenPosition(kGameOverRetryUIPosition);
	gameOverRetryUI_->SetSize(kGameOverUISize);
	gameOverRetryUI_->SetAnchorPoint(kGameOverUIAnchorPoint);
	// コールバックの設定
	gameOverRetryUI_->SetInteractable(true);
	gameOverRetryUI_->SetOnClickCallback([this]() {
		retry_ = true;
		ChangeState(SceneState::Exit);
		gameOverRetryUI_->SetInteractable(false);
										 });

	// タイトルフォントスプライトの初期化
	titleFontSprite_ = std::make_unique<FontSprite>();
	titleFontSprite_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	titleFontSprite_->SetText("Title");
	titleFontSprite_->SetPosition(kGameOverToTitleUIPosition);
	titleFontSprite_->SetScale(0.5f);

	// リトライフォントスプライトの初期化
	retryFontSprite_ = std::make_unique<FontSprite>();
	retryFontSprite_->Initialize(sceneManager_->GetSpriteCommon(), "luna");
	retryFontSprite_->SetText("Retry");
	retryFontSprite_->SetPosition(kGameOverRetryUIPosition);
	retryFontSprite_->SetScale(0.5f);

	StartState(SceneState::Enter);
}

void GameOverScene::Finalize()
{
}

void GameOverScene::Draw3D()
{
}

void GameOverScene::Draw2D()
{
	// UIの描画
	gameOverToTitleUI_->Draw();
	gameOverRetryUI_->Draw();

	// フォントスプライトの描画
	titleFontSprite_->Draw();
	retryFontSprite_->Draw();

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
	gameOverToTitleUI_->Update();
	gameOverRetryUI_->Update();
	titleFontSprite_->Update();
	retryFontSprite_->Update();
}