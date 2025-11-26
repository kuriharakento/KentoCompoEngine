#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "base/Camera.h"

/**
 * @brief カメラマネージャークラス
 * @details 複数のカメラを名前で管理し、アクティブカメラの切り替えを行う
 *          ImGuiを使用したカメラの位置・回転の編集機能を提供
 */
class CameraManager {
public:
	/**
	 * @brief カメラの追加
	 * @param name カメラの名前
	 */
    void AddCamera(const std::string& name);

    /**
     * @brief カメラの取得
     * @param name カメラの名前
     * @return カメラへのポインタ（見つからない場合はnullptr）
     */
    Camera* GetCamera(const std::string& name);

    /**
     * @brief アクティブカメラの設定
     * @param name アクティブにするカメラの名前
     */
    void SetActiveCamera(const std::string& name);

    /**
     * @brief 現在のアクティブカメラを取得
     * @return アクティブカメラへのポインタ
     */
	Camera* GetActiveCamera() { return activeCamera_; }

	/**
	 * @brief 現在のアクティブカメラの名前を取得
	 * @return アクティブカメラの名前
	 */
	const std::string& GetActiveCameraName() { return activeCameraName_; }

    /**
     * @brief 更新処理
     * @details アクティブカメラの更新とImGuiによるデバッグ表示を行う
     */
    void Update();

private:
    // カメラの名前とunique_ptrで管理されたカメラインスタンスのマップ
    std::unordered_map<std::string, std::unique_ptr<Camera>> cameras_;

    // 現在のアクティブカメラへのポインタ
    Camera* activeCamera_ = nullptr;

	// 現在のアクティブカメラの名前
	std::string activeCameraName_;
};