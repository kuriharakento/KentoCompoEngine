#include "SceneManager.h"
#include "engine/scene/factory/SceneFactory.h"
#include <assert.h>

#include "externals/imgui/imgui.h"
#include "effects/particle/ParticleManager.h"

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
	std::string startSceneName = SceneNames::Title;

	//最初のシーンを生成
	currentScene_ = sceneFactory_->CreateScene(startSceneName);
	currentScene_->SetSceneManager(this);
	currentScene_->Initialize();
	currentSceneName_ = startSceneName;
}

void SceneManager::Update()
{
#pragma region ImGui

#ifdef USE_IMGUI
	ImGui::Begin("SceneManager");
	ImGui::Text("CurrentScene: %s", currentSceneName_.c_str());
	if (ImGui::Button("Title"))
	{
		ChangeScene(SceneNames::Title);
	}
	if (ImGui::Button("GamePlay"))
	{
		ChangeScene(SceneNames::GamePlay);
	}
	if (ImGui::Button("GameOver"))
	{
		ChangeScene(SceneNames::GameOver);
	}
	if (ImGui::Button("GameClear"))
	{
		ChangeScene(SceneNames::GameClear);
	}
	// Debug用シーン
	if(ImGui::Button("StageEdit"))
	{
		ChangeScene(SceneNames::StageEdit);
	}
	if (ImGui::Button("ParticleTest"))
	{
		ChangeScene(SceneNames::ParticleTest);
	}
	if (ImGui::Button("EcsDebug (ECS 500 Cubes)"))
	{
		ChangeScene(SceneNames::EcsDebug);
	}
	// --- シーンのステートを直接変更するデバッグ UI ---
	if (currentScene_)
	{
		ImGui::SeparatorText("Scene State");
		const char* stateNames[] = { "None", "Enter", "Intro", "Playing", "Paused", "Cutscene", "End", "Exit" };
		int current = static_cast<int>(currentScene_->GetCurrentState());
		static int selectedState = current;
		// UI と内部状態を常に同期しておく
		if (selectedState != current) selectedState = current;
		if (ImGui::Combo("State", &selectedState, stateNames, IM_ARRAYSIZE(stateNames)))
		{
			currentScene_->DebugSetState(static_cast<SceneState>(selectedState));
		}
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

void SceneManager::DrawShadow()
{
	currentScene_->DrawShadow();
}

void SceneManager::DrawGBuffer()
{
	currentScene_->DrawGBuffer();
}

void SceneManager::ChangeScene(const std::string& sceneName)
{
	//nullチェック
	assert(sceneFactory_);
	assert(nextScene_ == nullptr);

	//次のシーンを生成
	nextScene_ = sceneFactory_->CreateScene(sceneName);
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

		// シーン切り替え時にパーティクルをすべてクリア
		ParticleManager::GetInstance()->Clear();

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
