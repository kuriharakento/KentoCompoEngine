#include "ParticleTestScene.h"

#include <numbers>

#include "effects/particle/component/group/UVTranslateComponent.h"
#include "effects/particle/component/single/ColorFadeOutComponent.h"
#include "effects/particle/component/single/GravityComponent.h"
#include "effects/particle/component/single/ScaleOverLifetimeComponent.h"
#include "manager/graphics/LineManager.h"
#include "manager/scene/CameraManager.h"
#include "math/VectorColorCodes.h"
#include "scene/manager/SceneManager.h"

void ParticleTestScene::Initialize()
{
	// シーン初期状態
	StartState(SceneState::Playing);

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(sceneManager_->GetCameraManager()->GetActiveCamera());
	debugCamera_->Start();

	// カメラの位置を調整
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetTranslate({ 0.0f, 5.0f, -20.0f });
	// カメラの向きを調整
	sceneManager_->GetCameraManager()->GetActiveCamera()->SetRotate({ 0.0f, 0.0f, 0.0f });

	// エミッタの生成と設定（Start は OnEnterEnter で呼ぶ）
	auraCylinder_ = std::make_unique<ParticleEmitter>();
	auraMist_ = std::make_unique<ParticleEmitter>();
	auraFloor_ = std::make_unique<ParticleEmitter>();
	auraLeak_ = std::make_unique<ParticleEmitter>();
	auraCylinder_->Initialize("auraCylinder", "./Resources/gradation.png");
	auraMist_->Initialize("auraMist", "./Resources/gradation.png");
	auraFloor_->Initialize("auraFloor", "./Resources/gradation.png");
	auraLeak_->Initialize("auraLeak", "./Resources/gradation.png");

	// 白い円柱設定
	auraCylinder_->SetModelType(ParticleGroup::ParticleType::Cylinder);
	auraCylinder_->SetEmitRange(Vector3(), Vector3());
	auraCylinder_->SetBillborad(false);
	auraCylinder_->SetInitialLifeTime(2.0f);
	auraCylinder_->SetEmitRate(1.0f);
	auraCylinder_->SetInitialRotation(Vector3(std::numbers::pi_v<float>, 0.0f, 0.0f));
	auraCylinder_->SetInitialScale(Vector3(0.3f, 10.0f, 0.3f));

	// モヤモヤ設定
	auraMist_->SetModelType(ParticleGroup::ParticleType::Plane);
	auraMist_->SetBillborad(true);
	auraMist_->SetEmitRange(Vector3(), Vector3());
	auraMist_->SetInitialLifeTime(1.5f);
	auraMist_->SetEmitRate(0.2f);
	auraMist_->SetInitialScale(Vector3(1.0f, 2.0f, 1.0f));
	auraMist_->SetInitialVelocity(Vector3(0.0f, 0.3f, 0.0f));
	auraMist_->SetInitialColor(VectorColorCodes::White - Vector4(0.0f, 0.0f, 0.0f, 0.7f));
	// コンポーネント追加
	auraMist_->AddComponent(std::make_shared<UVTranslateComponent>(Vector3(0.0f, 0.3f, 0.0f)));

	// 床に広がる光設定
	auraFloor_->SetModelType(ParticleGroup::ParticleType::Ring);
	auraFloor_->SetEmitRange(Vector3(), Vector3());
	auraFloor_->SetInitialLifeTime(1.0f);
	auraFloor_->SetEmitRate(1.0f);
	auraFloor_->SetRandomRotationRange(AABB{ Vector3{ std::numbers::pi_v<float> / 2.0f, -3.14f, 0.0f }, Vector3{ std::numbers::pi_v<float> / 2.0f, 3.14f, 0.0f } });
	// コンポーネント追加
	auraFloor_->AddComponent(std::make_shared<ScaleOverLifetimeComponent>(0.0f, 3.0f));
	auraFloor_->AddComponent(std::make_shared<ColorFadeOutComponent>());
}

void ParticleTestScene::Finalize()
{
}

//
// 状態フック
//

void ParticleTestScene::OnEnterPlaying()
{
	// シーン開始時にエミッタを実際に Start する
	auraCylinder_->Start(
		Vector3(0.0f, 10.0f, 0.0f),
		1,
		0.0f,
		true
	);

	auraMist_->Start(
		Vector3(0.0f, 0.0f, 0.0f),
		1,
		0.0f,
		true
	);

	auraFloor_->Start(
		Vector3(0.0f, 0.0f, 0.0f),
		1,
		0.0f,
		true
	);
}

void ParticleTestScene::OnUpdatePlaying()
{
	// デバッグカメラの更新
	if (debugCamera_) debugCamera_->Update();

}


void ParticleTestScene::Draw3D()
{
	// グリッドを描画
	LineManager::GetInstance()->DrawGrid(
		30.0f,
		5.0f,
		VectorColorCodes::White
	);
}

void ParticleTestScene::Draw2D()
{
	// 今のところ何も描画しない
}