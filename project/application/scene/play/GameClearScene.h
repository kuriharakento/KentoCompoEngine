#pragma once
#include "scene/interface/BaseScene.h"
#include <memory>
#include <vector>

// graphics
#include "graphics/3d/Object3d.h"
// manager
#include "manager/scene/LightManager.h"
#include <camerawork/debug/DebugCamera.h>

/**
 * @brief ゲームクリアシーン（シャドウテスト用）
 */
class GameClearScene : public BaseScene
{
public:
	void Initialize() override;
	void Finalize() override;
	void Draw2D() override;
	void DrawImGui() override;

protected:

	void OnEnterPlaying() override;
	void OnUpdatePlaying() override;
	void OnExitPlaying() override;

private:
	// 地面オブジェクト
	std::unique_ptr<Object3d> ground_;

	// テスト用オブジェクト（キューブなど）
	std::vector<std::unique_ptr<Object3d>> testObjects_;

	// デバッグカメラ
	DebugCamera debugCamera_;

	// オブジェクトの回転角度
	float objectRotation_ = 0.0f;
};
