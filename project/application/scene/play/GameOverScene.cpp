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
	
	if(transitionEffect_.GetState() == TransitionState::Done)
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
	if(Input::GetInstance()->TriggerKey(DIK_SPACE))
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
}

void GameOverScene::OnUpdateExit()
{
	transitionEffect_.Update();
	
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		sceneManager_->ChangeScene(SceneNames::Title);
	}
}

void GameOverScene::OnExitExit()
{
}