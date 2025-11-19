#include "GameClearScene.h"

#include "engine/scene/manager/SceneManager.h"
#include <input/Input.h>

void GameClearScene::Initialize()
{
	StartState(SceneState::Playing);
}

void GameClearScene::Finalize()
{
}

void GameClearScene::Draw3D()
{
}

void GameClearScene::Draw2D()
{
}

void GameClearScene::DrawImGui()
{
}

void GameClearScene::OnEnterPlaying()
{
}

void GameClearScene::OnUpdatePlaying()
{
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		sceneManager_->ChangeScene(SceneNames::Title);
	}
}

void GameClearScene::OnExitPlaying()
{
}