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
#include "application/minimap/Minimap.h"
#include "effects/particle/ParticleEmitter.h"
#include "graphics/2d/NumberSprite.h"

class GamePlayScene : public BaseScene
{
public:
    //初期化
    void Initialize() override;
    //終了
    void Finalize() override;
    //更新
    void Update() override;
    //描画
    void Draw3D() override;
    void Draw2D() override;

private:
    // ImGuiの描画
    void DrawImGui();

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
};