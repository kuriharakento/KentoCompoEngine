#pragma once
#include "camerawork/debug/DebugCamera.h"
#include "scene/interface/BaseScene.h"

/**
 * @brief パーティクルテストシーン
 */
class ParticleTestScene : public BaseScene
{
public:
	void Initialize() override;
	void Finalize() override;
	void Draw3D() override;
	void Draw2D() override;

protected:
	void OnEnterPlaying() override;
	void OnUpdatePlaying() override;

private:
	std::unique_ptr<DebugCamera> debugCamera_;
};