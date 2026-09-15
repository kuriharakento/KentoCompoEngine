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
#include "base/JobSystem.h"
#include "base/WinApp.h"
#include "manager/system/SrvManager.h"
// editor
#include "manager/editor/ImGuiManager.h"
// graphics
#include "graphics/3d/Skybox.h"
#include "graphics/2d/SpriteCommon.h"
#include "graphics/2d/GlyphAtlas.h"
#include "graphics/2d/TextOverlay.h"
#include "graphics/text/Text3DRenderer.h"
#include "graphics/3d/Object3dCommon.h"
// shadow
#include "manager/graphics/ShadowMapManager.h"
#include "graphics/shadow/ShadowMapPipeline.h"
// deferred
#include "graphics/deferred/DeferredRenderer.h"
// render pipeline
#include "graphics/pipeline/RenderPipeline.h"
#include "graphics/pipeline/RenderProfiler.h"
// view
#include "graphics/view/ISubViewProvider.h"
#include "graphics/view/RenderView.h"
#include "graphics/FrameConstantAllocator.h"
// atmosphere
#include "graphics/atmosphere/BeamRenderer.h"
#include "graphics/atmosphere/FogRenderer.h"
// npr
#include "graphics/npr/OutlineRenderer.h"
// screen quality
#include "graphics/atmosphere/VolumetricLightRenderer.h"
#include "graphics/postfx/DepthOfFieldRenderer.h"
#include "graphics/postfx/FxaaRenderer.h"
#include "graphics/view/PlanarReflection.h"

namespace KCE
{
/**
 * @brief フレームワーククラス
 * @details ゲームエンジンの基盤となるクラス。
 *          初期化、更新、描画、終了処理の流れを管理する。
 */
class Framework : public ISubViewProvider
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
	 * @brief 描画パイプラインを実行する
	 *
	 * @details シャドウからポストプロセス、2Dまでを一括で描く。
	 *          派生クラスの Draw() はこれを1回呼ぶだけでよい。
	 *          パスの構成を変えたい場合は GetRenderPipeline() から差し込む。
	 *
	 * @param outputTarget ポストプロセスの出力先。
	 *                     nullptr の場合はバックバッファへ直接描く。
	 *                     エディタではシーンをImGuiに表示するため
	 *                     レンダーターゲットを渡す。
	 */
	void ExecuteRenderPipeline(RenderTexture* outputTarget);

	/**
	 * @brief 描画パイプラインを取得する
	 * @details 新しいパスを差し込むときに使う。
	 * @return パイプライン
	 */
	RenderPipeline* GetRenderPipeline() { return renderPipeline_.get(); }

	/**
	 * @brief 本編を描くビューを取得する
	 * @return メインビュー
	 */
	RenderView* GetMainView() { return mainView_.get(); }

	/**
	 * @brief サブビューを1つ描く
	 *
	 * @details ステージモニターへの中継映像、カメラプレビュー小窓、
	 *          床の平面反射などに使う。シーンだけを描き、
	 *          結果は view->GetSceneColor() に残る。
	 *
	 *          シャドウマップは本編のパイプラインが作ったものを共有するので、
	 *          必ずメインの ExecuteRenderPipeline() より後に呼ぶこと。
	 *
	 *          ビューにカメラが設定されている場合、描画の間だけ
	 *          アクティブカメラを差し替える。既存の描画経路は一様に
	 *          アクティブカメラを見るため、この方法が最も影響が小さい。
	 *
	 * @param view 描画するビュー
	 */
	void RenderSubView(RenderView* view);

	/**
	 * @brief 毎フレーム描くサブビューを登録する
	 *
	 * @details 登録されたビューは、本編のシーンを描き終えた後・
	 *          ポストプロセスの前に描かれる。この位置なら
	 *          本編のシャドウマップを共有でき、かつ
	 *          レンダーターゲットの状態を踏み荒らさない。
	 *
	 *          所有権は持たない。ビューを破棄する前に必ず登録を解除すること。
	 *
	 * @param view 登録するビュー
	 */
	void RegisterSubView(RenderView* view);

	/**
	 * @brief サブビューの登録を解除する
	 * @param view 解除するビュー
	 */
	void UnregisterSubView(RenderView* view);

	/**
	 * @brief サブビューを作って登録する（所有は Framework）
	 * @details 作りたてのシーンカラーはレンダーターゲット状態なので、
	 *          一度描かれるまではテクスチャとして読まないこと。
	 */
	RenderView* CreateSubView(const std::string& name, uint32_t width, uint32_t height) override;

	/**
	 * @brief CreateSubView で作ったビューを登録解除して破棄する
	 * @details 登録はすぐ外すが、破棄は次のフレームの頭まで遅らせる。
	 *          エディタの Inspector などは、このフレームの描画コマンドを積んだ後に呼ぶことがあり、
	 *          すぐ消すと実行前のコマンドリストが消えたレンダーターゲットを指したままになる。
	 */
	void DestroySubView(RenderView* view) override;

	/**
	 * @brief シャドウマップの描画範囲を設定する
	 * @param nearPlane ニアクリップ距離
	 * @param farPlane ファークリップ距離
	 */
	void SetShadowRange(float nearPlane, float farPlane)
	{
		shadowNearPlane_ = nearPlane;
		shadowFarPlane_ = farPlane;
	}

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

protected:
	/**
	 * @brief ウィンドウサイズ変更時のコールバック
	 * @param width 新しい幅
	 * @param height 新しい高さ
	 */
	virtual void OnResize(uint32_t width, uint32_t height) {}

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
	// 読み込みを並べて回すワーカー。Finalize の最初に止める
	std::unique_ptr<JobSystem> jobSystem_;
	// スプライト共通部
	std::unique_ptr<SpriteCommon> spriteCommon_;
	// ゲーム内の日本語の文字（Windows のフォントから焼いたアトラス）
	std::unique_ptr<GlyphAtlas> glyphAtlas_;
	// 2D の文字の描き方。TextOverlay の文字が参照するので、それより先に宣言して後で壊れるようにする
	std::unique_ptr<TextSpritePipeline> textSpritePipeline_;
	// 歌詞テロップと会話枠。シーケンサの Text トラックが中身を決める
	std::unique_ptr<TextOverlay> textOverlay_;
	// 3D 空間の文字。文字列はシーンが持ち、名前で登録する
	std::unique_ptr<Text3DRenderer> text3DRenderer_;
	// 3Dオブジェクト共通部
	std::unique_ptr<Object3dCommon> objectCommon_;
	std::unique_ptr<FrameConstantAllocator> frameConstantAllocator_;
	// カメラマネージャー
	std::unique_ptr<CameraManager> cameraManager_;
	// シーンマネージャー
	std::unique_ptr<SceneManager> sceneManager_;
	// シーンファクトリ
	std::unique_ptr<SceneFactory> sceneFactory_;
	// ライトマネージャー
	std::unique_ptr<LightManager> lightManager_;
	// 本編を描くビュー（カメラ・G-Buffer・シーンカラーを束ねたもの）
	std::unique_ptr<RenderView> mainView_;
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
	// 大気フォグ
	std::unique_ptr<FogRenderer> fogRenderer_;
	// スポットライトのビーム
	std::unique_ptr<BeamRenderer> beamRenderer_;
	// アウトライン（輪郭線）
	std::unique_ptr<OutlineRenderer> outlineRenderer_;
	// FXAA（トーンマップ後のジャギーを均す）
	std::unique_ptr<FxaaRenderer> fxaaRenderer_;
	// 被写界深度
	std::unique_ptr<DepthOfFieldRenderer> depthOfFieldRenderer_;
	// スポットライトのボリュメトリック（レイマーチ）
	std::unique_ptr<VolumetricLightRenderer> volumetricLightRenderer_;
	// 床の平面反射。サブビューは Framework（ISubViewProvider）が持つ
	std::unique_ptr<PlanarReflection> planarReflection_;
	// 描画パイプライン（差し替え可能なパスの列）
	std::unique_ptr<RenderPipeline> renderPipeline_;
	// サブビュー用の、シーンだけを描くパイプライン
	std::unique_ptr<RenderPipeline> subViewPipeline_;
	// パスごとの GPU 時間とドローコール数の計測
	std::unique_ptr<RenderProfiler> renderProfiler_;
	// 毎フレーム描くサブビュー（所有しない）
	std::vector<RenderView*> subViews_;
	// CreateSubView で作ったビュー（こちらは所有する）
	std::vector<std::unique_ptr<RenderView>> ownedSubViews_;
	// DestroySubView で登録を外したが、まだ GPU が使っているかもしれないビュー。次のフレームの頭で破棄する
	std::vector<std::unique_ptr<RenderView>> pendingDestroySubViews_;
	// シャドウマップのニアクリップ距離
	float shadowNearPlane_ = 0.1f;
	// シャドウマップのファークリップ距離
	float shadowFarPlane_ = 200.0f;

private:
	/**
	 * @brief 描画パスへ渡すコンテキストを組み立てる
	 * @param outputTarget ポストプロセスの出力先
	 * @return 組み立てられたコンテキスト
	 */
	RenderPassContext MakeRenderPassContext(RenderView* view, RenderTexture* outputTarget) const;
};
} // namespace KCE
