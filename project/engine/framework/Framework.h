#pragma once

// effects
#include "base/RenderTexture.h"
#include "manager/effect/PostProcessManager.h"
// scene
#include "engine/scene/factory/SceneFactory.h"
#include "engine/scene/manager/SceneManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/scene/LightManager.h"
// system
#include "base/DirectXCommon.h"
#include "base/WinApp.h"
#include "manager/system/SrvManager.h"
// editor
#include "manager/editor/ImGuiManager.h"
// graphics
#include "graphics/3d/Skybox.h"
#include "graphics/2d/SpriteCommon.h"
#include "graphics/3d/Object3dCommon.h"
// shadow
#include "manager/graphics/ShadowMapManager.h"
#include "graphics/shadow/ShadowMapPipeline.h"
// deferred
#include "graphics/deferred/DeferredRenderer.h"

/**
 * @brief フレームワーククラス
 * @details ゲームエンジンの基盤となるクラス。
 *          初期化、更新、描画、終了処理の流れを管理する。
 */
class Framework
{
public: // メンバ関数
	/**
	 * @brief デストラクタ
	 */
	virtual ~Framework() = default;

	/**
	 * @brief 初期化
	 * @details 各種マネージャーやレンダリング設定を初期化する
	 */
	virtual void Initialize();

	/**
	 * @brief 終了処理
	 * @details 各種リソースを解放する
	 */
	virtual void Finalize();

	/**
	 * @brief 毎フレーム更新処理
	 * @details 入力、カメラ、シーンなどを更新する
	 */
	virtual void Update();

	/**
	 * @brief 描画処理
	 * @details 派生クラスで実装する
	 */
	virtual void Draw() = 0;

	/**
	 * @brief 3D描画用の設定
	 * @details 3Dオブジェクト描画の共通設定を行う
	 */
	void Draw3DSetting();

	/**
	 * @brief 2D描画用の設定
	 * @details スプライト描画の共通設定を行う
	 */
	void Draw2DSetting();

	/**
	 * @brief パフォーマンス情報の表示
	 * @details FPSとメモリ使用量を表示する
	 */
	void ShowPerformanceInfo();

	/**
	 * @brief 終了リクエストがあるか
	 * @return 終了リクエストフラグ
	 */
	virtual bool IsEndRequest() { return endRequest_; }

	/**
	 * @brief 実行
	 * @details メインループを実行する
	 */
	void Run();

protected: // メンバ変数
	// 終了リクエストフラグ
	bool endRequest_ = false;
	// ウィンドウアプリケーション
	std::unique_ptr<WinApp> winApp_;
	// DirectXCommon
	std::unique_ptr<DirectXCommon> dxCommon_;
	// SRVマネージャー
	std::unique_ptr<SrvManager> srvManager_;
	// ImGuiManager
	std::unique_ptr<ImGuiManager> imguiManager_;
	// スプライト共通部
	std::unique_ptr<SpriteCommon> spriteCommon_;
	// 3Dオブジェクト共通部
	std::unique_ptr<Object3dCommon> objectCommon_;
	// カメラマネージャー
	std::unique_ptr<CameraManager> cameraManager_;
	// シーンマネージャー
	std::unique_ptr<SceneManager> sceneManager_;
	// シーンファクトリ
	std::unique_ptr<SceneFactory> sceneFactory_;
	// ライトマネージャー
	std::unique_ptr<LightManager> lightManager_;
	// レンダーテクスチャ
	std::unique_ptr<RenderTexture> renderTexture_;
	// ポストプロセスマネージャー
	std::unique_ptr<PostProcessManager> postProcessManager_;
	// Skybox
	std::unique_ptr<Skybox> skybox_;
	// ブルーム用ブライトパスレンダーターゲット
	std::unique_ptr<RenderTexture> brightPassRT_;
	// ブラー用のレンダーターゲット（ピンポンバッファ）
	std::unique_ptr<RenderTexture> blurRT_[2];
	// シャドウマップマネージャー
	std::unique_ptr<ShadowMapManager> shadowMapManager_;
	// シャドウマップ描画パイプライン
	std::unique_ptr<ShadowMapPipeline> shadowMapPipeline_;
	// ディファードレンダラー
	std::unique_ptr<DeferredRenderer> deferredRenderer_;
};


