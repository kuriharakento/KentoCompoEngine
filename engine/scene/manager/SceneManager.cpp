#include "SceneManager.h"
#include "engine/scene/factory/SceneFactory.h"
#include <assert.h>

#include "externals/imgui/imgui.h"

SceneManager::~SceneManager()
{
	//現在のシーンを終了
	currentScene_->Finalize();
}

void SceneManager::Initialize(const SceneContext& context)
{
	//コンテキストをセット
	context_ = context;

	//初期シーンの名前
	std::string startSceneName = "TITLE";

	//最初のシーンを生成
	currentScene_.reset(sceneFactory_->CreateScene(startSceneName));
	currentScene_->SetSceneManager(this);
	currentScene_->Initialize();
	currentSceneName_ = startSceneName;
}

void SceneManager::Update()
{
#pragma region ImGui

#ifdef _DEBUG
	ImGui::Begin("SceneManager");
	ImGui::Text("CurrentScene: %s", currentSceneName_.c_str());
	if (ImGui::Button("Title"))
	{
		ChangeScene("TITLE");
	}
	if (ImGui::Button("GamePlay"))
	{
		ChangeScene("GAMEPLAY");
	}
	if(ImGui::Button("StageEdit"))
	{
		ChangeScene("STAGEEDIT");
	}
	ImGui::End();
#endif

#pragma endregion

	//次のシーンが予約されているか
	ReserveNextScene();

	//シーンの更新
	currentScene_->Update();
}

void SceneManager::Draw3D()
{
	currentScene_->Draw3D();
}

void SceneManager::Draw2D()
{
	currentScene_->Draw2D();
}

void SceneManager::ChangeScene(const std::string& sceneName)
{
	//nullチェック
	assert(sceneFactory_);
	assert(nextScene_ == nullptr);

	//次のシーンを生成
	nextScene_.reset(sceneFactory_->CreateScene(sceneName));
	//次のシーンの名前をセット
	nextSceneName_ = sceneName;
}

void SceneManager::ReserveNextScene()
{
	//次のシーンが予約されているなら
	if (nextScene_)
	{
		//現在のシーンを終了
		currentScene_->Finalize();
		currentScene_.reset();

		//シーンを切り替え
		currentScene_ = std::move(nextScene_);
		currentSceneName_ = nextSceneName_;
		nextScene_.reset();
		nextSceneName_ = "";
		//次のシーンを初期化
		currentScene_->SetSceneManager(this);
		currentScene_->Initialize();
	}
}
