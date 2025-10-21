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
    // Enter: シーン開始（フェードインなど）
    void OnEnterEnter() override;
    void OnUpdateEnter() override;
    void OnExitEnter() override;

    // Playing: 実プレイ（既存 Update のロジックをここに移動）
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

private: //メンバ変数
    // ミニマップ
    std::unique_ptr<Minimap> minimap_;
    // スカイドーム
    std::unique_ptr<Object3d> skydome_;
    // 地面
    std::unique_ptr<Object3d> ground_;
    // カーネージモード
    std::unique_ptr<CarnageMode> carnageMode_;
    // カメラワーク
    std::unique_ptr<DebugCamera> debugCamera_;
    std::unique_ptr<SplineCamera> splineCamera_;
    std::unique_ptr<TopDownCamera> topDownCamera_;
    // ゲームオブジェクト
    std::unique_ptr<Player> player;
    std::unique_ptr<EnemyManager> enemyManager_;
    std::unique_ptr<ObstacleManager> obstacleManager_;
    std::unique_ptr<StageManager> stageManager_;
    // シーン遷移エフェクト
    SceneTransitionEffect transitionEffect_;
    // ゲーム終了フラグ
    bool gameEnd_ = false;
};