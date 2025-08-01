#pragma once
#include <memory>

// app
#include "application/GameObject/character/enemy/EnemyManager.h"
#include "application/GameObject/character/player/Player.h"
#include "application/GameObject/obstacle/ObstacleManager.h"

// camerawork
#include "camerawork/debug/DebugCamera.h"
#include "camerawork/spline/SplineCamera.h"
#include "camerawork/topDown/TopDownCamera.h"

// scene
#include "engine/scene/interface/BaseScene.h"

// graphics
#include "graphics/3d/Object3d.h"

// effects
#include "application/stage/StageManager.h"
#include "effects/particle/ParticleEmitter.h"

enum class TitleSceneState
{
	Cameraintro,
	Playing,
	NextScene,
};

class TitleScene : public BaseScene
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
	// パーティクルエミッターの初期化
	void InitializeParticleEmitters();
	// ImGuiの描画
	void DrawImGui();

private: //メンバ変数
	// シーンの状態
	TitleSceneState state_ = TitleSceneState::Cameraintro;
	// スカイドーム
	std::unique_ptr<Object3d> skydome_;
	// 地面
	std::unique_ptr<Object3d> ground_;
	// カメラワーク
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<SplineCamera> splineCamera_;
	std::unique_ptr<TopDownCamera> topDownCamera_;
	// ゲームオブジェクト
	std::unique_ptr<Player> player;
	std::unique_ptr<EnemyManager> enemyManager_;
	std::unique_ptr<ObstacleManager> obstacleManager_;
	std::unique_ptr<StageManager> stageManager_;
	// エミッター
	std::unique_ptr<ParticleEmitter> dust_;
	std::unique_ptr<ParticleEmitter> redEffect_;
	std::unique_ptr<ParticleEmitter> fallHeart_;
	std::unique_ptr<ParticleEmitter> glitch_;
	std::unique_ptr<ParticleEmitter> mordeVFXGround_;
	std::unique_ptr<ParticleEmitter> mordeVFXFragment_;
};
