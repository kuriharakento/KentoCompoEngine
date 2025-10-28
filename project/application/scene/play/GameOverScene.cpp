#include "GameOverScene.h"

// scene
#include "engine/scene/manager/SceneManager.h"
// math
#include "math/VectorColorCodes.h"
#include <input/Input.h>

void GameOverScene::Initialize()
{
	// シーン遷移エフェクトの初期化
	transitionEffect_.Initialize(
		sceneManager_->GetSpriteCommon(),
		"./Resources/black.png",
		22, 16,
		1280.0f, 720.0f
	);
	
	// シーン状態を Enter から開始
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
	// シーン遷移エフェクトの描画
	transitionEffect_.Draw();
}

void GameOverScene::DrawImGui()
{
}

void GameOverScene::OnEnterEnter()
{
	// 開始時のフェードイン
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

void GameOverScene::OnEnterPlaying()
{
}

void GameOverScene::OnUpdatePlaying()
{
	if(Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		// 終了演出へ
		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::EdgesToCenter);
		transitionEffect_.Start(
			1.0f,
			VectorColorCodes::Black,
			VectorColorCodes::Red
		);
		// シーン終了
		ChangeState(SceneState::Exit);
	}
}

void GameOverScene::OnExitPlaying()
{
}

void GameOverScene::OnEnterExit()
{
}

void GameOverScene::OnUpdateExit()
{
	transitionEffect_.Update();
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		// タイトルシーンへ遷移
		sceneManager_->ChangeScene(SceneNames::Title);
	}
}

void GameOverScene::OnExitExit()
{
}