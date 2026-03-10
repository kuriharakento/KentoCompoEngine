#pragma once
#include <memory>

// app
#include "application/gameObject/combatable/character/enemy/EnemyManager.h"
#include "application/gameObject/combatable/character/player/Player.h"
#include "application/gameObject/obstacle/ObstacleManager.h"
#include "application/stage/StageManager.h"
// camerawork
#include "camerawork/debug/DebugCamera.h"
#include "camerawork/spline/SplineCamera.h"
#include "camerawork/topDown/TopDownCamera.h"

// scene
#include "engine/scene/interface/BaseScene.h"

// graphics
#include "graphics/3d/Object3d.h"

// effects
#include "application/carnage/CarnageMode.h"
#include "application/effect/CinematicLetterbox.h"
#include "application/effect/SceneTransitionEffect.h"
#include "application/minimap/Minimap.h"
#include "effects/particle/ParticleEmitter.h"
#include "graphics/2d/NumberSprite.h"
#include "camerawork/orbit/OrbitCameraWork.h"
#include "application/ui/GameUI.h"
#include "application/ui/Cursor.h"
#include "application/ui/ControlsGuide.h"
#include "application/ui/PoseMenu.h"

/**
 * @brief メインゲームプレイシーン
 * 
 * プレイヤーが敵を倒しながらステージをクリアするメインゲームシーンです。
 * ステージ管理、敵管理、コンボシステム、カーネージモード、カメラワーク、
 * UIなどのゲームプレイに必要な全ての要素を統合管理します。
 * 
 * シーン状態遷移: Enter(開始演出) → Intro(イントロ) → Playing(プレイ) → End(終了演出) → Exit(退場)
 * 
 * @note カメラはスプラインカメラとトップダウンカメラを状況に応じて切り替え可能
 * @note ゲームクリア・ゲームオーバー判定を行い、適切なシーンへ遷移
 */
class GamePlayScene : public BaseScene
{
public:
    /**
     * @brief シーンの初期化処理
     * 
     * BGM再生、カメラ設定、スカイドーム、地面、ステージ、プレイヤー、敵、
     * カーネージモード、ミニマップなど、ゲームプレイに必要な全要素を初期化します。
     */
    void Initialize() override;
    
    /**
     * @brief シーンの終了処理
     * 
     * BGMの停止、各種リソースの解放を行います。
     */
    void Finalize() override;

    /**
     * @brief 3D描画処理
     * 
     * スカイドーム、地面、ステージ、プレイヤー、敵、障害物などの3Dオブジェクトを描画します。
     */
    void Draw3D() override;

	/**
	 * @brief シャドウ描画処理
	 * 
	 * シャドウマップへの描画を行います。
	 */
	void DrawShadow() override;
    
    /**
     * @brief 2D描画処理
     * 
     * UI要素（ミニマップ、コンボ表示、レターボックスエフェクト等）を描画します。
     */
    void Draw2D() override;

    /**
     * @brief ImGuiデバッグUI描画
     * 
     * 開発用のデバッグ情報を表示します。
     */
    void DrawImGui() override;

protected:
    // ==================================================
    // 状態フックのオーバーライド
    // ==================================================
    
    /**
     * @brief Enter状態開始時の処理
     * 
     * シーン開始時のフェードイン演出を開始します。
     */
    void OnEnterEnter() override;
    
    /**
     * @brief Enter状態の更新処理
     * 
     * フェードイン演出の進行を管理し、完了後にIntro状態へ遷移します。
     */
    void OnUpdateEnter() override;
    
    /**
     * @brief Enter状態終了時の処理
     * 
     * Enter状態からの退場処理を行います。
     */
    void OnExitEnter() override;

	/**
	 * @brief Intro状態開始時の処理
	 * 
	 * ゲーム開始前のイントロ演出（カメラワーク等）を開始します。
	 */
	void OnEnterIntro() override;
	
	/**
	 * @brief Intro状態の更新処理
	 * 
	 * イントロ演出を進行させ、完了後にPlaying状態へ遷移します。
	 */
	void OnUpdateIntro() override;

    /**
     * @brief Playing状態開始時の処理
     * 
     * 実際のゲームプレイを開始します。カメラをトップダウンビューに切り替えます。
     */
    void OnEnterPlaying() override;
    
    /**
     * @brief Playing状態の更新処理
     * 
     * プレイヤー、敵、ステージ、当たり判定、コンボ、カーネージモードなど、
     * ゲームプレイの中核となる全ての要素を更新します。
     * ゲームクリア・ゲームオーバー判定もここで行います。
     */
    void OnUpdatePlaying() override;
    
    /**
     * @brief Playing状態終了時の処理
     * 
     * Playing状態からの退場処理を行います。
     */
    void OnExitPlaying() override;

    /**
     * @brief End状態開始時の処理
     * 
     * ゲームクリアまたはゲームオーバー時の終了演出を開始します。
     */
    void OnEnterEnd() override;
    
    /**
     * @brief End状態の更新処理
     * 
     * 終了演出を進行させ、完了後にExit状態へ遷移します。
     */
    void OnUpdateEnd() override;
    
    /**
     * @brief End状態終了時の処理
     * 
     * End状態からの退場処理を行います。
     */
    void OnExitEnd() override;

    /**
     * @brief Exit状態開始時の処理
     * 
     * シーン退場時のフェードアウト演出を開始します。
     */
    void OnEnterExit() override;
    
    /**
     * @brief Exit状態の更新処理
     * 
     * フェードアウト演出を進行させ、完了後に次のシーン（GameClearまたはGameOver）へ遷移します。
     */
    void OnUpdateExit() override;
    
    /**
     * @brief Exit状態終了時の処理
     * 
     * Exit状態からの退場処理を行います。
     */
    void OnExitExit() override;

	/**
	 * @brief 全状態共通の更新処理
	 * 
	 * シーン遷移エフェクト、レターボックスエフェクト、カメラなど、
	 * 状態に関わらず常に更新が必要な要素を処理します。
	 */
	void CommonUpdate() override;

private:
	// =========================
	//  シーン設定定数
	// =========================

	// オーディオ
	static constexpr float kBgmVolume = 0.2f;
	static constexpr float KSeVolume = 0.3f;

	// ライティング
	static constexpr Vector3 kLightDirection = { -0.4f, -1.0f, 1.0f };
	static constexpr float kLightIntensity = 0.3f;
	static constexpr float kSkydomeLightIntensity = 0.5f;
	static constexpr float kSkydomeScale = 0.5f;

	// スプラインカメラ
	static constexpr float kSplineCameraSpeed = 0.001f;

	// トップダウンカメラ
	static constexpr float kTopDownCameraPitch = 0.7f;
	static constexpr float kTopDownCameraYaw = 1.0f;
	static constexpr float kTopDownCameraHeight = 43.0f;

	// コントロールガイド
	static constexpr float kControlsGuideScale = 0.3f;

	// トランジション演出
	static constexpr int kTransitionGridX = 22;
	static constexpr int kTransitionGridY = 16;
	static constexpr float kEnterTransitionDuration = 1.5f;
	static constexpr float kExitTransitionDuration = 2.0f;

	// ゲームオーバー演出
	static constexpr float kGameOverFrequencyHz = 4.0f;
	static constexpr float kGameOverMaxOscAmp = 35.0f;
	static constexpr float kGameOverDecayRate = 2.8f;
	static constexpr float kGameOverEnvelopeThreshold = 0.001f;

	// ゲームクリア演出
	static constexpr float kOrbitCameraDistance = 15.0f;
	static constexpr float kOrbitCameraSpeed = 0.5f;
	static constexpr float kOrbitAngleOffsetDeg = -30.0f;
	static constexpr float kLetterboxShowDuration = 1.0f;
	static constexpr float kClearToExitDelay = 2.0f;

	// =========================
    //  ゲームプレイ
	// =========================
    
    // ミニマップUI（敵・エリアの位置表示）
    std::unique_ptr<Minimap> minimap_;
    // レティクル
	std::unique_ptr<Cursor> reticle_;
    // スカイドーム（背景天球）
    std::unique_ptr<Object3d> skydome_;
    // 地面オブジェクト
    std::unique_ptr<Object3d> ground_;
    // カーネージモード（コンボ達成時の強化システム）
    std::unique_ptr<CarnageMode> carnageMode_;
    // スプラインカメラ（演出用）
    std::unique_ptr<SplineCamera> splineCamera_;
    // デバッグカメラ
    std::unique_ptr<DebugCamera> debugCamera_;
    // デバッグカメラの有効フラグ
    bool isDebugCameraActive_ = false;
    // トップダウンカメラ（ゲームプレイ用）
    std::unique_ptr<TopDownCamera> topDownCamera_;
    // オービットカメラワーク（デバッグ用）
	std::unique_ptr<OrbitCameraWork> orbitCamera_;
    // 敵管理
    std::unique_ptr<EnemyManager> enemyManager_;
    // 障害物管理
    std::unique_ptr<ObstacleManager> obstacleManager_;
    // ステージ・プレイヤー管理
    std::unique_ptr<StageManager> stageManager_;
    // シーン遷移エフェクト（フェードイン/アウト）
    SceneTransitionEffect transitionEffect_;
    // レターボックスエフェクト（映画的演出）
	CinematicLetterbox cinematicLetterbox_;
    // ゲームオーバー演出の持続時間（秒）
    float gameOverEffectDuration_ = 3.0f;
    // ゲームオーバー演出の経過時間（秒）
	float gameOverEffectElapsed_ = 0.0f;

    // ゲームクリアフラグ
    bool gameClear_ = false;
    // ゲームオーバーフラグ
	bool gameOver_ = false;

	// 操作説明UI
	std::unique_ptr<ControlsGuide> controlsGuide_;

	// ポーズメニュー
	std::unique_ptr<PoseMenu> poseMenu_;

	// ========================
	//  イントロ演出
	// ========================

    // イントロ演出の経過時間（秒）
    float introElapsed_ = 0.0f;
    // イントロ演出の所要時間（秒）
	float introDuration_ = 2.0f;
    // カメラ初期位置
    Vector3 cameraInitialPosition_ = { -82.0f, 50.0f, 100.0f };
    // カメラ初期回転
	Vector3 cameraInitialRotation_ = { 0.45f, 1.545f, 0.0f };
};