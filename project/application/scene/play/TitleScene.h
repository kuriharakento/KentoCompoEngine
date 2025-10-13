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
#include "application/effect/TitleFireEffect.h"
#include "application/minimap/Minimap.h"
#include "effects/particle/ParticleEmitter.h"
#include "graphics/2d/NumberSprite.h"

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
	// ImGuiの描画
	void DrawImGui();

private: //メンバ変数
	// タイトルロゴ
	std::unique_ptr<Sprite> titleLogo_;
	// スカイドーム
	std::unique_ptr<Object3d> skydome_;
	// 炎エフェクト
	std::unique_ptr<TitleFireEffect> fireEffect_;
	// キューブ
	OBB cube_{};
	float cubeRotateY = 0.0f;
	float cubeWaveTime = 0.0f;
	//
	SceneTransitionEffect transitionEffect_;
};
