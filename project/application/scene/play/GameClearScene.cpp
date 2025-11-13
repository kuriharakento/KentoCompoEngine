#include "GameClearScene.h"

#include "engine/scene/manager/SceneManager.h"
#include <input/Input.h>

void GameClearScene::Initialize()
{
	// Playing状態から開始（クリア画面表示）
	StartState(SceneState::Playing);
}

void GameClearScene::Finalize()
{
	// リソース解放処理（現状は特になし）
}

void GameClearScene::Draw3D()
{
	// 3D要素の描画（現状は特になし）
}

void GameClearScene::Draw2D()
{
	// クリア画面UIの描画（将来実装）
}

void GameClearScene::DrawImGui()
{
	// デバッグ情報表示（現状は特になし）
}

void GameClearScene::OnEnterPlaying()
{
	// Playing状態の初期化処理（現状は特になし）
}

void GameClearScene::OnUpdatePlaying()
{
	// スペースキーでタイトルシーンへ戻る
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		sceneManager_->ChangeScene(SceneNames::Title);
	}
}

void GameClearScene::OnExitPlaying()
{
	// Playing状態の退場処理（現状は特になし）
}