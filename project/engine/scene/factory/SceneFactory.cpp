#include "SceneFactory.h"

#include "application/scene/debug/ParticleTestScene.h"
#include "application/scene/debug/StageEditScene.h"
#include "application/scene/play/GameClearScene.h"
#include "application/scene/play/GameOverScene.h"
#include "application/scene/play/GamePlayScene.h"
#include "application/scene/play/TitleScene.h"
#include "base/Logger.h"

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	//次のシーンを生成
	std::unique_ptr<BaseScene> newScene = nullptr;

	if (sceneName == "TITLE")
	{
		newScene = std::make_unique<TitleScene>();
	}
	else if (sceneName == "GAMEPLAY")
	{
		newScene = std::make_unique<GamePlayScene>();
	}
	else if (sceneName == "GAMEOVER")
	{
		newScene = std::make_unique<GameOverScene>();
	}
	else if (sceneName == "GAMECLEAR")
	{
		newScene = std::make_unique<GameClearScene>();
	}
	// デバッグ用シーン
	else if(sceneName == "STAGEEDIT")
	{
		newScene = std::make_unique<StageEditScene>();
	}
	else if (sceneName == "PARTICLETEST")
	{
		newScene = std::make_unique<ParticleTestScene>();
	}
	else
	{
		//名前のシーンがない場合
		Logger::Log("Can't Create Scene\n");
	}

	return newScene;
}
