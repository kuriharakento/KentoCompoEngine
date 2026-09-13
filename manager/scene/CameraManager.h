#pragma once
#include "editor/SelectionContext.h"
#include <string>
#include <unordered_map>
#include <memory>
#include "base/Camera.h"

namespace KCE
{
class DirectXCommon;

/**
 * @brief カメラマネージャークラス
 * @details 複数のカメラを名前で管理し、アクティブカメラの切り替えを行う
 *          ImGuiを使用したカメラの位置・回転の編集機能を提供
 */
class CameraManager {
public:
	/**
	 * @brief デストラクタ
	 */
	~CameraManager();

	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonへのポインタ（カメラのGPUバッファ初期化用）
	 */
	void Initialize(DirectXCommon* dxCommon);

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
	Camera* GetActiveCamera() { return renderCameraOverride_ ? renderCameraOverride_ : activeCamera_; }

	/**
	 * @brief 描画中だけアクティブカメラを差し替える
	 *
	 * @details サブビュー（中継映像、カメラプレビュー、反射）を描くときに使う。
	 *          既存の描画経路は一様に GetActiveCamera() からカメラを引くため、
	 *          呼び出し側を全て書き換えるより、描画の間だけ差し替えるほうが安全。
	 *
	 *          @b 必ず ClearRenderCameraOverride() と対で使うこと。
	 *          差し替えたまま更新処理へ抜けると、ゲームのロジックが
	 *          サブビューのカメラを見てしまう。
	 *
	 * @param camera 差し替えるカメラ。nullptr なら差し替えない
	 */
	void SetRenderCameraOverride(Camera* camera) { renderCameraOverride_ = camera; }

	/**
	 * @brief 描画用のカメラ差し替えを解除する
	 */
	void ClearRenderCameraOverride() { renderCameraOverride_ = nullptr; }

	/**
	 * @brief 差し替えを無視して、本来のアクティブカメラを取得する
	 * @return アクティブカメラ
	 */
	Camera* GetPrimaryCamera() const { return activeCamera_; }

	/**
	 * @brief 現在のアクティブカメラの名前を取得
	 * @return アクティブカメラの名前
	 */
	const std::string& GetActiveCameraName() { return activeCameraName_; }

    /**
     * @brief 更新処理
     * @details アクティブカメラの更新
     */
    void Update();

#ifdef USE_IMGUI
	/**
	 * @brief カメラの一覧を描く。Hierarchy に出し、選んだら SelectionContext へ伝える
	 */
	void DrawHierarchyImGui();

	/**
	 * @brief 選んだカメラ1つの詳細を描く。Inspector に出す
	 * @param item SelectionKind::Camera の選択
	 */
	void DrawInspectorImGui(const SelectionItem& item);
#endif

private:
    // カメラの名前とunique_ptrで管理されたカメラインスタンスのマップ
    std::unordered_map<std::string, std::unique_ptr<Camera>> cameras_;

    // 現在のアクティブカメラへのポインタ
    Camera* activeCamera_ = nullptr;

    // 描画中だけアクティブカメラを差し替えるためのポインタ
    Camera* renderCameraOverride_ = nullptr;

	// 現在のアクティブカメラの名前
	std::string activeCameraName_;

	// DirectXCommonへのポインタ（カメラのGPUバッファ初期化用）
	DirectXCommon* dxCommon_ = nullptr;
};
} // namespace KCE
