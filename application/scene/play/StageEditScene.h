#pragma once

// graphics
#include "graphics/3d/Object3d.h"
// scene
#include "scene/interface/BaseScene.h"
// camera
#include "camerawork/debug/DebugCamera.h"
// app
#include "application/GameObject/obstacle/ObstacleManager.h"

class StageEditScene : public  BaseScene
{
public:
	void Initialize() override;
	void Update() override;
	void Draw2D() override;
	void Draw3D() override;
	void Finalize() override;

private:
	//スカイドーム
	std::unique_ptr<Object3d> skydome_;
	// 障害物マネージャー
	std::unique_ptr<ObstacleManager> obstacleManager_;
	// デバッグカメラ
	std::unique_ptr<DebugCamera> debugCamera_;
};

