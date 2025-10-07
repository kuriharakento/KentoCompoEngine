#pragma once
#include <memory>
#include <vector>
#include "effects/particle/ParticleEmitter.h"
#include "math/Vector3.h"

// タイトル画面用炎柱エフェクト管理クラス
class TitleFireEffect
{
public:
    void Initialize();
    void Update(const Vector3& cameraPos); // カメラ位置を渡す
    void EmitFire(const Vector3& position);

private:
    std::unique_ptr<ParticleEmitter> fireEmitterRight_;
    std::unique_ptr<ParticleEmitter> fireEmitterLeft_;
    std::unique_ptr<ParticleEmitter> floorEmitter_;
    Vector3 floorPos = {};
    float lastFireZ_ = 0.0f; // 炎を出した最後のZ
    const float interval_ = 1.0f;
    const float laneOffset_ = 1.5f;
    const float groundY_ = -0.5f;
	float time = 0.0f;
	const std::string fireTexturePath_ = "./Resources/gradation.png";
	bool firstUpdate_ = true;
};