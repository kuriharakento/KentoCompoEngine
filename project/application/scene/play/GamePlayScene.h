#pragma once
#include <memory>

// app
#include "application/GameObject/Combatable/character/enemy/EnemyManager.h"
#include "application/GameObject/Combatable/character/player/Player.h"
#include "application/GameObject/obstacle/ObstacleManager.h"
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
#include "application/effect/PlayerDeathEffect.h"
#include "application/effect/SceneTransitionEffect.h"
#include "application/minimap/Minimap.h"
#include "effects/particle/ParticleEmitter.h"
#include "graphics/2d/NumberSprite.h"

class GamePlayScene : public BaseScene
{
public:
    // 初期化 / 終了
    void Initialize() override;
    void Finalize() override;

    // 描画
    void Draw3D() override;
    void Draw2D() override;

    // ImGui の描画（BaseScene::DrawImGui をオーバーライド）
    void DrawImGui() override;

protected:
    // 状態フックのオーバーライド
    // Enter: シーン開始（フェードイン）
    void OnEnterEnter() override;
    void OnUpdateEnter() override;
    void OnExitEnter() override;

	// Intro: イントロ演出（ゲームのスタート演出)
	void OnEnterIntro() override;
	void OnUpdateIntro() override;

    // Playing: 実プレイ（ゲームプレイ)
    void OnEnterPlaying() override;
    void OnUpdatePlaying() override;
    void OnExitPlaying() override;

    // End: ステージクリア等の終了演出（必要に応じて拡張）
    void OnEnterEnd() override;
    void OnUpdateEnd() override;
    void OnExitEnd() override;

    // Exit: シーン退場（フェードアウト → シーン遷移）
    void OnEnterExit() override;
    void OnUpdateExit() override;
    void OnExitExit() override;

	// 共通更新処理
	void CommonUpdate() override;

private: //メンバ変数
	// =========================
    //  ゲームプレイ
	// =========================
    
    // ミニマップ
    std::unique_ptr<Minimap> minimap_;
    // スカイドーム
    std::unique_ptr<Object3d> skydome_;
    // 地面
    std::unique_ptr<Object3d> ground_;
    // カーネージモード
    std::unique_ptr<CarnageMode> carnageMode_;
    // カメラワーク
    std::unique_ptr<SplineCamera> splineCamera_;
    std::unique_ptr<TopDownCamera> topDownCamera_;
    // ゲームオブジェクト
    std::unique_ptr<EnemyManager> enemyManager_;
    std::unique_ptr<ObstacleManager> obstacleManager_;
    std::unique_ptr<StageManager> stageManager_;
    // シーン遷移エフェクト
    SceneTransitionEffect transitionEffect_;
	// レターボックスエフェクト
	CinematicLetterbox cinematicLetterbox_;
	// プレイヤー死亡エフェクト
	PlayerDeathEffect playerDeathEffect_;
    // ゲームオーバー演出持続時間
	float gameOverEffectDuration_ = 3.0f;
	// ゲームオーバー演出経過時間
	float gameOverEffectElapsed_ = 0.0f;

    // ゲーム終了フラグ
    bool gameClear_ = false;
	bool gameOver_ = false;

	// ========================
	//  イントロ演出
	// ========================

    // イントロ演出の経過時間
	float introElapsed_ = 0.0f;
	// イントロ演出の所要時間
	float introDuration_ = 2.0f;
    // カメラの初期値
    Vector3 cameraInitialPosition_ = { 0.0f, 1.5f, 50.0f };
	Vector3 cameraInitialRotation_ = { 00.0f, 0.0f, 0.0f };
};