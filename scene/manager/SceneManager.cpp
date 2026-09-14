#include "SceneManager.h"
#include "engine/scene/factory/SceneFactory.h"
#include <assert.h>

#include "externals/imgui/imgui.h"
#include "effects/particle/ParticleManager.h"
#include "manager/editor/DebugUIManager.h"
#include "manager/graphics/ModelManager.h"
#include "manager/graphics/TextureManager.h"
#include "manager/system/SrvManager.h"

namespace KCE
{
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
	std::string startSceneName = "TitleScene";

	//最初のシーンを生成
	currentScene_ = sceneFactory_->CreateScene(startSceneName);
	currentScene_->SetSceneManager(this);
	currentScene_->Initialize();
	currentSceneName_ = startSceneName;

#ifdef USE_IMGUI
	// シーンマネージャーをデバッグUIに登録
	DebugUIManager::GetInstance()->RegisterHierarchySection(this, "シーン管理", [this]() {
		ImGui::Text("CurrentScene: %s", currentSceneName_.c_str());
		ImGui::Text("Textures: resident %zu / scene %zu",
			TextureManager::GetInstance()->GetResidentTextureCount(),
			TextureManager::GetInstance()->GetSceneTextureCount());
		ImGui::Text("Models: resident %zu / scene %zu",
			ModelManager::GetInstance()->GetResidentModelCount(),
			ModelManager::GetInstance()->GetSceneModelCount());
		ImGui::Text("Active SRVs: %u", context_.srvManager ? context_.srvManager->GetActiveSRVCount() : 0);

		// 任意文字列によるシーン直接切り替え入力欄（末尾の "Scene" は自動付加）
		static char inputSceneName[128] = "";
		ImGui::SeparatorText("シーンを直接切り替える");
		ImGui::InputText("シーン名", inputSceneName, sizeof(inputSceneName));
		ImGui::SameLine();
		if (ImGui::Button("切り替え") && inputSceneName[0] != '\0')
		{
			std::string targetScene = inputSceneName;
			ChangeScene(targetScene);
		}

		ImGui::SeparatorText("登録済みのシーン");
		auto registeredScenes = SceneFactory::GetRegisteredSceneNames();
		for (const auto& sceneName : registeredScenes)
		{
			// ボタン表示用ラベル（末尾の "Scene" を取り除いて表示）
			std::string label = sceneName;
			if (label.size() >= 5 && label.substr(label.size() - 5) == "Scene")
			{
				label = label.substr(0, label.size() - 5);
			}

			if (ImGui::Button(label.c_str()))
			{
				ChangeScene(label);
			}
		}

		// --- シーンのステート表示（デバッグ UI） ---
		if (currentScene_)
		{
			ImGui::SeparatorText("シーンの状態");
			ImGui::Text("State: %s", currentScene_->GetCurrentStateName().c_str());
		}
	});
#endif
}

void SceneManager::Update()
{

	//次のシーンが予約されているか
	ReserveNextScene();

	//シーンの更新
	currentScene_->Update();
}

void SceneManager::DrawTransparent()
{
	currentScene_->DrawTransparent(context_.cameraManager);
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

	// Sceneという文字列をつけてシーン名を作成
	const std::string fullSceneName = sceneName + "Scene";

	//次のシーンを生成
	nextScene_ = sceneFactory_->CreateScene(fullSceneName);
	//次のシーンの名前をセット
	nextSceneName_ = fullSceneName;
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

		// 旧シーンの描画がGPUで終わってから、参照されなくなった素材と番号を返す。
		if (context_.dxCommon)
		{
			context_.dxCommon->ExecuteAndWait();
			ModelManager::GetInstance()->ReleaseSceneResources();
			TextureManager::GetInstance()->ReleaseSceneResources();
		}

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
} // namespace KCE
