#pragma once
#include "camerawork/debug/DebugCamera.h"
#include "effects/particle/ParticleEmitter.h"
#include "scene/interface/BaseScene.h"

class ParticleTestScene : public BaseScene
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
	// デバッグカメラ
	std::unique_ptr<DebugCamera> debugCamera_;

	// 白いオーラエフェクトにチャレンジ
	// 白い円柱
	std::unique_ptr<ParticleEmitter> auraCylinder_;
	// モヤモヤ
	std::unique_ptr<ParticleEmitter> auraMist_;
	// 床に広がる光
	std::unique_ptr<ParticleEmitter> auraFloor_;
	// 円柱から漏れる粒
	std::unique_ptr<ParticleEmitter> auraLeak_;
};

