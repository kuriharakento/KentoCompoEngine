#pragma once

// graphics
#include "graphics/3d/Object3d.h"
// scene
#include "scene/interface/BaseScene.h"
// camera
#include "camerawork/debug/DebugCamera.h"
// app
#include "application/stage/StageManager.h"

/**
 * @brief ステージ編集シーン
 * 
 * ゲームステージのレイアウトを編集するための開発用シーンです。
 * 敵の配置、障害物の配置、エリアの設定などをビジュアルに行えます。
 * 
 * @note デバッグカメラで自由に視点を変更できます
 * @note 編集内容はファイルに保存され、GamePlaySceneでロードされます
 * @note 開発用シーンであり、本番ビルドには含まれません
 */
class StageEditScene : public BaseScene
{
public:
	/**
	 * @brief シーンの初期化処理
	 * 
	 * デバッグカメラ、ステージマネージャーを初期化します。
	 */
	void Initialize() override;
	
	/**
	 * @brief シーンの終了処理
	 * 
	 * 各種リソースの解放を行います。
	 */
	void Finalize() override;
	
	/**
	 * @brief 2D描画処理
	 * 
	 * UI要素を描画します。
	 */
	void Draw2D() override;
	
	/**
	 * @brief 3D描画処理
	 * 
	 * ステージ、配置オブジェクトを描画します。
	 */
	void Draw3D() override;

protected:
	/**
	 * @brief Playing状態の更新処理
	 * 
	 * デバッグカメラの更新、ImGuiによるステージ編集インターフェースを提供します。
	 * オブジェクトの配置、移動、削除などの操作を行えます。
	 */
	void OnUpdatePlaying() override;

private:
	std::unique_ptr<StageManager> stageManager_; ///< ステージデータ管理
	std::unique_ptr<DebugCamera> debugCamera_; ///< デバッグ用フリーカメラ
};