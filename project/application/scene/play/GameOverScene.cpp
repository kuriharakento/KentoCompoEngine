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
	
	// Enter状態から開始（フェードイン演出）
	StartState(SceneState::Enter);
}

void GameOverScene::Finalize()
{
	// リソース解放処理（現状は特になし）
}

void GameOverScene::Draw3D()
{
	// 3D要素の描画（現状は特になし）
}

void GameOverScene::Draw2D()
{
	// シーン遷移エフェクトの描画
	transitionEffect_.Draw();
}

void GameOverScene::DrawImGui()
{
	// デバッグ情報表示（現状は特になし）
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
	
	// フェードイン完了でPlaying状態へ遷移
	if(transitionEffect_.GetState() == TransitionState::Done)
	{
		ChangeState(SceneState::Playing);
	}
}

void GameOverScene::OnExitEnter()
{
	// Enter状態の退場処理（現状は特になし）
}

// ==================================================
// Playing状態（ゲームオーバー表示・入力待ち）
// ==================================================
void GameOverScene::OnEnterPlaying()
{
	// Playing状態の初期化処理（現状は特になし）
}

void GameOverScene::OnUpdatePlaying()
{
	// スペースキーでタイトルへ戻る
	if(Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		// 終了演出（フェードアウト）の開始
		transitionEffect_.SetEaseType(SceneTransitionEase::InSine);
		transitionEffect_.SetFadeType(FadeType::FadeIn);
		transitionEffect_.SetMode(TransitionMode::EdgesToCenter);
		transitionEffect_.Start(
			1.0f,
			VectorColorCodes::Black,
			VectorColorCodes::Red
		);
		// Exit状態へ遷移
		ChangeState(SceneState::Exit);
	}
}

void GameOverScene::OnExitPlaying()
{
	// Playing状態の退場処理（現状は特になし）
}

// ==================================================
// Exit状態（シーン退場・タイトルへ遷移）
// ==================================================
void GameOverScene::OnEnterExit()
{
	// Exit状態の初期化処理（現状は特になし）
}

void GameOverScene::OnUpdateExit()
{
	transitionEffect_.Update();
	
	// フェードアウト完了でタイトルシーンへ遷移
	if (transitionEffect_.GetState() == TransitionState::Done)
	{
		sceneManager_->ChangeScene(SceneNames::Title);
	}
}

void GameOverScene::OnExitExit()
{
	// Exit状態の退場処理（現状は特になし）
}