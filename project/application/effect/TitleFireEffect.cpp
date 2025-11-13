#include "TitleFireEffect.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/ScaleOverLifetimeComponent.h"
#include "effects/particle/component/single/RotationComponent.h"
#include "effects/particle/component/single/AccelerationComponent.h"
#include "effects/particle/component/single/DragComponent.h"
#include "effects/particle/component/single/RandomInitialVelocityComponent.h"
#include "math/VectorColorCodes.h"
#include "time/TimeManager.h"

void TitleFireEffect::Initialize()
{
    fireEmitterLeft_ = std::make_unique<ParticleEmitter>();
    fireEmitterLeft_->Initialize("TitleFire_Left", fireTexturePath_);
    fireEmitterLeft_->SetBlendMode(BlendMode::Additive);
	fireEmitterLeft_->SetModelType(ParticleGroup::ParticleType::Plane);
    fireEmitterLeft_->SetInitialLifeTime(0.6f);
	fireEmitterLeft_->SetEmitRate(0.1f);
    fireEmitterLeft_->SetRandomScale(true);
	fireEmitterLeft_->SetRandomScaleRange(AABB(Vector3(0.01f, 0.01f, 0.01f), Vector3(0.2f, 0.2f, 0.2f)));
    fireEmitterLeft_->SetInitialColor({ 0.8f, 0.1f, 0.1f, 0.95f }); // 赤黒
	fireEmitterLeft_->SetEmitRange(Vector3(-0.3f, 0.0f, -2.0f), Vector3(0.3f, 0.0f, 2.0f));
    fireEmitterLeft_->SetRandomVelocity(true);
	fireEmitterLeft_->SetRandomVelocityRange(AABB(Vector3(-0.5f, 0.5f, -0.5f), Vector3(0.5f, 2.0f, 0.5f)));
    fireEmitterLeft_->SetBillborad(true);
    fireEmitterLeft_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    fireEmitterLeft_->AddComponent(std::make_shared<RotationComponent>(Vector3(0.0f, 0.25f, 0.0f)));
    fireEmitterLeft_->AddComponent(std::make_shared<DragComponent>(0.87f));
    fireEmitterLeft_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 1.0f, 0.0f))); // 上昇

    fireEmitterRight_ = std::make_unique<ParticleEmitter>();
    fireEmitterRight_->Initialize("TitleFire_Right", fireTexturePath_);
    fireEmitterRight_->SetBlendMode(BlendMode::Additive);
	fireEmitterRight_->SetModelType(ParticleGroup::ParticleType::Plane);
    fireEmitterRight_->SetInitialLifeTime(0.5f);
    fireEmitterRight_->SetEmitRate(0.1f);
    fireEmitterRight_->SetRandomScale(true);
    fireEmitterRight_->SetRandomScaleRange(AABB(Vector3(0.01f, 0.01f, 0.01f), Vector3(0.2f, 0.2f, 0.2f)));
    fireEmitterRight_->SetInitialColor({ 0.8f, 0.1f, 0.1f, 0.95f }); // 赤黒
    fireEmitterRight_->SetEmitRange(Vector3(-0.3f, 0.0f, -2.0f), Vector3(0.3f, 0.0f, 2.0f));
    fireEmitterRight_->SetRandomVelocity(true);
    fireEmitterRight_->SetRandomVelocityRange(AABB(Vector3(-0.5f, 0.5f, -0.5f), Vector3(0.5f, 2.0f, 0.5f)));
    fireEmitterRight_->SetBillborad(true);
    fireEmitterRight_->AddComponent(std::make_shared<ColorFadeOutComponent>());
    fireEmitterRight_->AddComponent(std::make_shared<RotationComponent>(Vector3(0.0f, 0.25f, 0.0f)));
    fireEmitterRight_->AddComponent(std::make_shared<DragComponent>(0.87f));
    fireEmitterRight_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 1.0f, 0.0f))); // 上昇

	floorEmitter_ = std::make_unique<ParticleEmitter>();
	floorEmitter_->Initialize("titleFlorParticle", "./Resources/circle2.png");
	floorEmitter_->SetInitialColor(VectorColorCodes::Salmon);
	floorEmitter_->SetRandomColor(true);
	floorEmitter_->SetRandomColorRange(VectorColorCodes::Black,VectorColorCodes::Red);
	floorEmitter_->SetInitialScale(Vector3(0.01f, 0.01f, 0.01f));
	floorEmitter_->SetEmitRate(0.2f);
	floorEmitter_->SetEmitRange(Vector3(-30.0f, 0.0f, 0.0f), Vector3(30.0f, 1.0f, 60.0f));
	floorEmitter_->SetRandomScale(true);
	floorEmitter_->SetRandomScaleRange(AABB(Vector3(0.001f, 0.001f, 0.001f), Vector3(0.2f, 0.2f, 0.2f)));
	floorEmitter_->SetInitialLifeTime(1.0f);
	floorEmitter_->SetBillborad(true);
	// コンポーネントの追加
	floorEmitter_->AddComponent(std::make_shared<DragComponent>(0.95f));
	floorEmitter_->AddComponent(std::make_shared<ColorFadeOutComponent>());
	floorEmitter_->AddComponent(std::make_shared<AccelerationComponent>(Vector3(0.0f, 0.3f, 0.0f)));
	floorEmitter_->Start(
		&floorPos,
		20,
		0.0f,
		true
	);
	firstUpdate_ = false;
}

void TitleFireEffect::Update(const Vector3& cameraPos)
{
	// 床面エフェクトをカメラ位置に追従
	floorPos = cameraPos;
	floorPos.y = groundY_;
	
	// タイマー更新と炎発生判定
    if (time >= 0.0f)
    {
        time -= TimeManager::GetInstance().GetGameContext().deltaTime;
	}
	else
	{
		// インターバル経過後、新たな炎柱を発生
        time = interval_;
        
        EmitFire(cameraPos);
        lastFireZ_ = cameraPos.z;
	}
}

void TitleFireEffect::EmitFire(const Vector3& position)
{
	// カメラ前方の左右位置に炎柱を配置
	Vector3 leftPos = position + Vector3(-laneOffset_, groundY_, 10.0f);
	Vector3 rightPos = position + Vector3(laneOffset_, groundY_, 10.0f);
	
	fireEmitterRight_->Start(rightPos, 7, 0.5f, false);
	fireEmitterLeft_->Start(leftPos, 7, 0.5f, false);
}