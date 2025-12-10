#include "CameraManager.h"
#include <iostream>

// system
#include "base/Logger.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void CameraManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
}

void CameraManager::AddCamera(const std::string& name) {
    // 名前で重複を避けてカメラを追加
    if (cameras_.find(name) == cameras_.end()) {
        // 新しいカメラをunique_ptrで生成してマップに追加
        cameras_[name] = std::make_unique<Camera>();
        Logger::Log("Add Camera: " + name + "\n");

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

#ifdef USE_IMGUI
    /*--------------[ ImGuiでのデバッグ表示 ]-----------------*/
	ImGui::Begin("CameraManager");

	// アクティブカメラの名前を表示
	ImGui::Text("Active Camera: %s", activeCameraName_.c_str());

    // カメラのリストを表示
    if(ImGui::CollapsingHeader("list"))
    {
		for (auto& camera : cameras_)
		{
            ImGui::Text("Camera: %s", camera.first.c_str());
		}
    }

	// カメラの位置を編集
	Vector3 cameraPosition = activeCamera_->GetTranslate();
	ImGui::DragFloat3("translate", &cameraPosition.x, 0.1f);
	activeCamera_->SetTranslate(cameraPosition);

	// カメラの回転を編集
	Vector3 cameraRotate = activeCamera_->GetRotate();
	ImGui::DragFloat3("rotate", &cameraRotate.x, 0.01f, -3.14f, 3.14f);
	activeCamera_->SetRotate(cameraRotate);

    // カメラの追加ボタン
	if (ImGui::Button("Add Camera"))
	{
		AddCamera("camera" + std::to_string(cameras_.size()));
	}

	// アクティブカメラの切り替えボタン
	for (auto& camera : cameras_)
	{
		if (ImGui::Button(camera.first.c_str()))
		{
			SetActiveCamera(camera.first);
		}
	}

    ImGui::End();
#endif

    // アクティブカメラを更新
    activeCamera_->Update();
    
}
