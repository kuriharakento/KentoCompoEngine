#include "SceneFactory.h"

#include <unordered_map>

#include "application/scene/debug/ParticleTestScene.h"
#include "application/scene/debug/StageEditScene.h"
#include "application/scene/play/GameClearScene.h"
#include "application/scene/play/GameOverScene.h"
#include "application/scene/play/GamePlayScene.h"
#include "application/scene/play/TitleScene.h"
#include "base/Logger.h"

namespace
{
// シーン生成関数の型
using SceneCreator = std::unique_ptr<BaseScene>(*)();

// テンプレートで生成関数を作る
template<typename T>
std::unique_ptr<BaseScene> CreateSceneImpl()
{
	return std::make_unique<T>();
}

// シーン名 → 生成関数のテーブル（新シーン追加時はここに1行追加）
const std::unordered_map<std::string, SceneCreator> kSceneTable = {
	{"TITLE",        CreateSceneImpl<TitleScene>},
	{"GAMEPLAY",     CreateSceneImpl<GamePlayScene>},
	{"GAMEOVER",     CreateSceneImpl<GameOverScene>},
	{"GAMECLEAR",    CreateSceneImpl<GameClearScene>},
	{"STAGEEDIT",    CreateSceneImpl<StageEditScene>},
	{"PARTICLETEST", CreateSceneImpl<ParticleTestScene>},
};
}

std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	auto it = kSceneTable.find(sceneName);
	if (it != kSceneTable.end())
	{
		return it->second();
	}

	Logger::Log("Can't Create Scene\n");
	return nullptr;
}
