#pragma once

// graphics
#include "graphics/3d/Object3d.h"
// scene
#include "scene/interface/BaseScene.h"
// camera
#include "camerawork/debug/DebugCamera.h"
// app
#include "application/stage/StageManager.h"

class StageEditScene : public  BaseScene
{
public:
	void Initialize() override;
	void Update() override;
	void Draw2D() override;
	void Draw3D() override;
	void Finalize() override;

private:
	// ステージマネージャー
	std::unique_ptr<StageManager> stageManager_;
	// デバッグカメラ
	std::unique_ptr<DebugCamera> debugCamera_;
};

