#include "SceneFactory.h"

#include "application/scene/debug/ParticleTestScene.h"
#include "application/scene/debug/StageEditScene.h"
#include "application/scene/play/GameClearScene.h"
#include "application/scene/play/GameOverScene.h"
#include "application/scene/play/GamePlayScene.h"
#include "application/scene/play/TitleScene.h"
#include "base/Logger.h"

BaseScene* SceneFactory::CreateScene(const std::string& sceneName)
{
	//次のシーンを生成
	BaseScene* newScene = nullptr;

	if (sceneName == "TITLE")
	{
		newScene = new TitleScene();
	}
	else if (sceneName == "GAMEPLAY")
	{
		newScene = new GamePlayScene();
	}
	else if (sceneName == "GAMEOVER")
	{
		newScene = new GameOverScene();
	}
	else if (sceneName == "GAMECLEAR")
	{
		newScene = new GameClearScene();
	}
	// デバッグ用シーン
	else if(sceneName == "STAGEEDIT")
	{
		newScene = new StageEditScene();
	}
	else if (sceneName == "PARTICLETEST")
	{
		newScene = new ParticleTestScene();
	}
	else
	{
		//名前のシーンがない場合
		Logger::Log("Can't Create Scene\n");
	}

	return newScene;
}
