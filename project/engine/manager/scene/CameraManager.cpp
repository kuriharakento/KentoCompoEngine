#include "CameraManager.h"
#include <iostream>

// system
#include "base/Logger.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

void CameraManager::AddCamera(const std::string& name) {
    // 名前で重複を避けてカメラを追加
    if (cameras_.find(name) == cameras_.end()) {
        // 新しいカメラをunique_ptrで生成してマップに追加
        cameras_[name] = std::make_unique<Camera>(); // unique_ptrでインスタンスを生成
        Logger::Log("Add Camera: " + name + "\n");

    }
}

Camera* CameraManager::GetCamera(const std::string& name) {
    auto it = cameras_.find(name);
    if (it != cameras_.end()) {
    	// 名前に対応するカメラが見つかった場合、そのポインタを返す
        return it->second.get();
    }
    return nullptr;
}

void CameraManager::SetActiveCamera(const std::string& name) {
    Camera* camera = GetCamera(name);
    if (camera) {
		// アクティブカメラを設定
        activeCamera_ = camera;
		activeCameraName_ = name;
    } else {
       
    }
}

void CameraManager::Update() {
    if(!activeCamera_)
    {
		// アクティブカメラが設定されていない場合は何もしない
        return;
    }

#ifdef _DEBUG
	ImGui::Begin("CameraManager");
	// アクティブカメラの名前
	ImGui::Text("Active Camera: %s", activeCameraName_.c_str());
    //カメラのリスト
    if(ImGui::CollapsingHeader("list"))
    {
		for (auto& camera : cameras_)
		{
            ImGui::Text("Camera: %s", camera.first.c_str());
		}
    }
	// カメラの位置
	Vector3 cameraPosition = activeCamera_->GetTranslate();
	ImGui::DragFloat3("translate", &cameraPosition.x, 0.1f);
	activeCamera_->SetTranslate(cameraPosition);
	// カメラの回転
	Vector3 cameraRotate = activeCamera_->GetRotate();
	ImGui::DragFloat3("rotate", &cameraRotate.x, 0.01f, -3.14f, 3.14f);
	activeCamera_->SetRotate(cameraRotate);

    //カメラの追加
	if (ImGui::Button("Add Camera"))
	{
		AddCamera("camera" + std::to_string(cameras_.size()));
	}

	//アクティブカメラの設定
	for (auto& camera : cameras_)
	{
		if (ImGui::Button(camera.first.c_str()))
		{
			SetActiveCamera(camera.first);
		}
	}

    ImGui::End();
#endif

    activeCamera_->Update();
    
}
