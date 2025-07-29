#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "base/Camera.h"

class CameraManager {
public:
	// 名前だけでカメラを追加
    void AddCamera(const std::string& name);

    // カメラを取得
    Camera* GetCamera(const std::string& name);

    // アクティブなカメラを設定
    void SetActiveCamera(const std::string& name);

    // 現在のアクティブカメラを取得
	Camera* GetActiveCamera() { return activeCamera_; }

	// 現在のアクティブカメラの名前を取得
	const std::string& GetActiveCameraName() { return activeCameraName_; }

    // 更新処理
    void Update();

private:
    // カメラの名前とunique_ptrで管理されたカメラインスタンスのマップ
    std::unordered_map<std::string, std::unique_ptr<Camera>> cameras_;

    // 現在のアクティブカメラ
    Camera* activeCamera_ = nullptr;

	//現在のアクティブカメラの名前
	std::string activeCameraName_;
};