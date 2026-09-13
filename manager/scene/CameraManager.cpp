#include "CameraManager.h"
#include <iostream>

// system
#include "base/Logger.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"

#endif

namespace KCE
{

void CameraManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;

#ifdef USE_IMGUI
	// 一覧は Hierarchy、選んだカメラの詳細は Inspector に出す
	DebugUIManager::GetInstance()->RegisterHierarchySection(this, "Cameras", [this]() { this->DrawHierarchyImGui(); });
	DebugUIManager::GetInstance()->RegisterInspector(this, SelectionKind::Camera, [this](const SelectionItem& item) { this->DrawInspectorImGui(item); });
#endif
}

CameraManager::~CameraManager()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
}

void CameraManager::AddCamera(const std::string& name) {
    // 名前で重複を避けてカメラを追加
    if (cameras_.find(name) == cameras_.end()) {
        // 新しいカメラをunique_ptrで生成してマップに追加
        cameras_[name] = std::make_unique<Camera>();
        KCE::Logger::Log("Add Camera: " + name + "\n");

		// GPU定数バッファを初期化
		if (dxCommon_)
		{
			cameras_[name]->InitializeConstantBuffer(dxCommon_);
		}
    }
}

Camera* CameraManager::GetCamera(const std::string& name) {
    // 名前でカメラを検索
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
    	// 名前に対応するカメラが見つかった場合、そのポインタを返す
        return it->second.get();
    }
    // 見つからない場合はnullptrを返す
    return nullptr;
}

void CameraManager::SetActiveCamera(const std::string& name) {
    // 名前からカメラを取得
    Camera* camera = GetCamera(name);
    if (camera) {
		// アクティブカメラを設定
        activeCamera_ = camera;
		activeCameraName_ = name;
    } else {
        // カメラが見つからない場合は何もしない
    }
}

void CameraManager::Update() {
    // アクティブカメラが設定されていない場合は何もしない
    if(!activeCamera_)
    {
        return;
    }

    // アクティブカメラを更新
    activeCamera_->Update();
}

#ifdef USE_IMGUI
void CameraManager::DrawHierarchyImGui()
{
	// 選ばれているかは今の選択と直接比べる（毎フレーム選択を作らない）
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	for (const auto& [name, camera] : cameras_)
	{
		(void)camera;
		ImGui::PushID(name.c_str());
		const bool isSelected = primary.kind == SelectionKind::Camera && primary.name == name;
		if (ImGui::Selectable(name.c_str(), isSelected))
		{
			SelectionItem item;
			item.kind = SelectionKind::Camera;
			item.name = name;
			SelectionContext::GetInstance()->Select(item);
		}
		if (name == activeCameraName_)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(Active)");
		}
		ImGui::PopID();
	}

	// カメラの追加ボタン
	if (ImGui::Button("Add Camera"))
	{
		AddCamera("camera" + std::to_string(cameras_.size()));
	}
}

void CameraManager::DrawInspectorImGui(const SelectionItem& item)
{
	// 選んだ後で消されていることがあるので、名前から引き直す
	Camera* camera = GetCamera(item.name);
	if (!camera)
	{
		ImGui::TextDisabled("このカメラは見つかりません");
		return;
	}

	ImGui::SeparatorText(item.name.c_str());
	if (item.name == activeCameraName_)
	{
		ImGui::TextUnformatted("Active Camera");
	}
	else if (ImGui::Button("Set Active"))
	{
		SetActiveCamera(item.name);
	}

	// 触ったときだけ書き戻す。毎フレーム書くと、シーケンサーがクォータニオンで回したカメラをオイラー角に戻してしまう
	Vector3 cameraPosition = camera->GetTranslate();
	if (ImGui::DragFloat3("translate", &cameraPosition.x, 0.1f))
	{
		camera->SetTranslate(cameraPosition);
	}
	Vector3 cameraRotate = camera->GetRotate();
	if (ImGui::DragFloat3("rotate", &cameraRotate.x, 0.01f, -3.14f, 3.14f))
	{
		camera->SetRotate(cameraRotate);
	}
}
#endif
} // namespace KCE
