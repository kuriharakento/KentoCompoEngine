#include "SceneManager.h"
#include "engine/scene/factory/SceneFactory.h"
#include <assert.h>

#include "base/Logger.h"
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
#ifdef USE_IMGUI
	// メニューや小窓から消えた this を呼ばないように外す
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
	//現在のシーンを終了
	if (currentScene_)
	{
		currentScene_->Finalize();
	}
}

void SceneManager::SetStartSceneName(const std::string& sceneName)
{
	startSceneName_ = sceneName.ends_with(sceneStr) ? sceneName : sceneName + sceneStr;
}

void SceneManager::Initialize(const SceneContext& context)
{
	//コンテキストをセット
	context_ = context;

	const std::string& startSceneName = startSceneName_;

	//最初のシーンを生成
	currentScene_ = sceneFactory_->CreateScene(startSceneName);
	// 最初のシーンが無いと何も動かせないので、ここは止める（名前はログに出ている）
	assert(currentScene_ && "start scene is not registered");
	currentScene_->SetSceneManager(this);
	currentScene_->Initialize();
	currentSceneName_ = startSceneName;

#ifdef USE_IMGUI
	// シーンマネージャーをデバッグUIに登録
	DebugUIManager::GetInstance()->RegisterHierarchySection(this, "シーンマネージャー", [this]() {
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
	});

	// メニューバーの「シーン」と、切り替え前の確認
	DebugUIManager::GetInstance()->RegisterMainMenu(this, "シーン", [this]() { DrawSceneMenu(); });
	DebugUIManager::GetInstance()->RegisterDialog(this, [this]() { DrawSceneChangeDialog(); });
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

bool SceneManager::ChangeScene(const std::string& sceneName)
{
	assert(sceneFactory_);

	// "Title" と "TitleScene" のどちらで来ても "TitleScene" にそろえる
	const std::string fullSceneName = sceneName.ends_with(sceneStr) ? sceneName : sceneName + sceneStr;

	// 名前の打ち間違いで今のシーンを捨てないよう、予約の時点で弾く
	if (!SceneFactory::HasScene(fullSceneName))
	{
		Logger::Log("[SceneManager Error] Scene is not registered: '" + fullSceneName + "'\n");
		return false;
	}

	if (HasPendingScene() && nextSceneName_ != fullSceneName)
	{
		Logger::Log("[SceneManager] Scene change overwritten: '" + nextSceneName_ + "' -> '" + fullSceneName + "'\n");
	}

	// シーンを作るのは切り替えのときなので、ここでは名前だけ覚える
	nextSceneName_ = fullSceneName;
	return true;
}

void SceneManager::ReserveNextScene()
{
	//次のシーンが予約されているなら
	if (HasPendingScene())
	{
		const std::string nextName = nextSceneName_;
		nextSceneName_.clear();

		// 今のシーンを畳む前に作っておく。作れなければ今のシーンのまま続ける
		std::unique_ptr<BaseScene> nextScene = sceneFactory_->CreateScene(nextName);
		if (!nextScene)
		{
			return;
		}

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
		currentScene_ = std::move(nextScene);
		currentSceneName_ = nextName;
		//次のシーンを初期化
		currentScene_->SetSceneManager(this);
		currentScene_->Initialize();
	}
}

#ifdef USE_IMGUI
namespace
{
constexpr const char* kSceneChangePopupName = "保存していない変更があります";
}

void SceneManager::RequestSceneChangeFromMenu(const std::string& sceneName)
{
	if (currentScene_ && currentScene_->HasUnsavedChanges())
	{
		pendingSceneName_ = sceneName;
		sceneChangePopupRequested_ = true;
		return;
	}
	ChangeScene(sceneName);
}

void SceneManager::DrawSceneMenu()
{
	// 一覧はメニューを開いた瞬間だけ作る（開いている間も毎フレーム作らない）
	if (ImGui::IsWindowAppearing() || menuSceneNames_.empty())
	{
		menuSceneNames_ = SceneFactory::GetRegisteredSceneNames();
		for (std::string& name : menuSceneNames_)
		{
			if (name.ends_with(sceneStr))
			{
				name.resize(name.size() - sceneStr.size());
			}
		}
	}

	// 今のシーン名は "〇〇Scene" なので、同じ形にして比べる
	for (const std::string& name : menuSceneNames_)
	{
		const bool isCurrent = currentSceneName_.size() == name.size() + sceneStr.size()
			&& currentSceneName_.starts_with(name) && currentSceneName_.ends_with(sceneStr);
		if (ImGui::MenuItem(name.c_str(), nullptr, isCurrent))
		{
			RequestSceneChangeFromMenu(name);
		}
	}

	ImGui::Separator();
	if (ImGui::MenuItem("今のシーンを読み直す") && currentSceneName_.ends_with(sceneStr))
	{
		RequestSceneChangeFromMenu(currentSceneName_.substr(0, currentSceneName_.size() - sceneStr.size()));
	}
}

void SceneManager::DrawSceneChangeDialog()
{
	if (sceneChangePopupRequested_)
	{
		ImGui::OpenPopup(kSceneChangePopupName);
		sceneChangePopupRequested_ = false;
	}
	if (!ImGui::BeginPopupModal(kSceneChangePopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::Text("今のシーンに保存していない変更があります。");
	ImGui::Text("切り替えると変更は失われます。");
	ImGui::Separator();
	if (ImGui::Button("保存しないで切り替える"))
	{
		if (!pendingSceneName_.empty())
		{
			ChangeScene(pendingSceneName_);
		}
		pendingSceneName_.clear();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("キャンセル"))
	{
		pendingSceneName_.clear();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}
#endif
} // namespace KCE
