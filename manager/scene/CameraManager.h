#pragma once
#include "editor/SelectionContext.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
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
	 * @brief 名前付きカメラを削除する。
	 * @param name カメラの名前
	 * @return 削除できたら真。アクティブカメラは削除しない
	 */
	bool RemoveCamera(const std::string& name);

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
	Camera* GetActiveCamera()
	{
		if (renderCameraOverride_)
		{
			return renderCameraOverride_;
		}
		return viewCameraOverride_ ? viewCameraOverride_ : activeCamera_;
	}

	/**
	 * @brief 画面に映すカメラを、解除するまで差し替える（デバッグカメラ用）
	 *
	 * @details アクティブカメラ（GetPrimaryCamera）には触らないので、シーンやカットシーンが
	 *          SetActiveCamera しても差し替えは外れない。解除すればその時点の本来のカメラに戻る。
	 *          サブビューの描画中は SetRenderCameraOverride の方が優先される。
	 * @param camera 差し替えるカメラ（所有しない）。CameraManager が持つカメラを渡す
	 */
	void SetViewCameraOverride(Camera* camera) { viewCameraOverride_ = camera; }

	/** @brief 画面に映すカメラの差し替えを解除する */
	void ClearViewCameraOverride() { viewCameraOverride_ = nullptr; }

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
	/** @brief 登録カメラのデバッグ形状をラインへ積む。 */
	void DrawDebugLines();

	/**
	 * @brief カメラごとに、視錐台のデバッグラインを出すか決める。
	 * @param name カメラの名前
	 * @param visible 出すなら true（既定は出す）
	 * @details ふだん使わないカメラ（SequenceCamera など）は隠しておいて、使うときだけ Inspector で出す。
	 */
	void SetDebugLineVisible(const std::string& name, bool visible);

	/** @brief そのカメラの視錐台を出すか */
	bool IsDebugLineVisible(const std::string& name) const { return !hiddenDebugLineNames_.contains(name); }

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
	/** @brief カメラの設定ページ（デバッグ線の表示）を描く */
	void DrawSettingsImGui();
#endif

private:
	/**
	 * @brief 画面から消してよいカメラかを返す。
	 * @param name カメラの名前
	 * @return 画面の「カメラを追加」で作っていて、アクティブでも描画中でもなければ真
	 */
	bool CanRemoveFromEditor(const std::string& name) const;

private:
    // カメラの名前とunique_ptrで管理されたカメラインスタンスのマップ
    std::unordered_map<std::string, std::unique_ptr<Camera>> cameras_;

    // 現在のアクティブカメラへのポインタ
    Camera* activeCamera_ = nullptr;

    // 描画中だけアクティブカメラを差し替えるためのポインタ
    Camera* renderCameraOverride_ = nullptr;
	// 解除するまで画面に映すカメラ（デバッグカメラ）。所有は cameras_
	Camera* viewCameraOverride_ = nullptr;

	// 現在のアクティブカメラの名前
	std::string activeCameraName_;

	// DirectXCommonへのポインタ（カメラのGPUバッファ初期化用）
	DirectXCommon* dxCommon_ = nullptr;
	// カメラの位置・向き・視錐台の線を出すか（Settings の「シーン > カメラ」で切り替える）
	bool drawDebugLines_ = true;
	// 画面の「カメラを追加」で作ったカメラの名前。
	// ほかのシステム（モニター・反射・カットシーン等）はカメラのポインタを持ち続けるので、ここにあるものだけ画面から消せる
	std::unordered_set<std::string> editorCameraNames_;
	// 視錐台を出さないカメラの名前
	std::unordered_set<std::string> hiddenDebugLineNames_;
};
} // namespace KCE
