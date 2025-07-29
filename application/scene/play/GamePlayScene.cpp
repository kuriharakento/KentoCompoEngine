#include "GamePlayScene.h"

// system
#include "input/Input.h"
// scene
#include "engine/scene/manager/SceneManager.h"


void GamePlayScene::Initialize()
{

}

void GamePlayScene::Finalize()
{

}

void GamePlayScene::Update()
{
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		sceneManager_->ChangeScene("TITLE");
	}
}

void GamePlayScene::Draw3D()
{

}

void GamePlayScene::Draw2D()
{

}
