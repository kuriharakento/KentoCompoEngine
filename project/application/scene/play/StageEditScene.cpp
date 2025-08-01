#include "StageEditScene.h"

// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"
// scene
#include "scene/manager/SceneManager.h"

void StageEditScene::Initialize()
{
	// スカイドームの初期化
	skydome_ = std::make_unique<Object3d>();
	skydome_->Initialize(sceneManager_->GetObject3dCommon());
	skydome_->SetModel("skydome.obj");
	skydome_->SetLightManager(sceneManager_->GetLightManager());
	skydome_->SetEnableLighting(true);
	skydome_->SetDirectionalLightIntensity(0.5f);
	//ディレクショナルライトを下から上に照らす
	skydome_->SetDirectionalLightDirection({ 0.0f, -1.0f, 0.0f });

	// 障害物マネージャーの初期化
	obstacleManager_ = std::make_unique<ObstacleManager>();
	obstacleManager_->Initialize(sceneManager_->GetObject3dCommon(), sceneManager_->GetLightManager());

	//デバッグカメラの初期化
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(
		sceneManager_->GetCameraManager()->GetActiveCamera()
	);
	debugCamera_->Start();
}

void StageEditScene::Update()
{
	// デバッグカメラの更新
	debugCamera_->Update();

	// スカイドームの更新
	skydome_->Update(sceneManager_->GetCameraManager());

	// 障害物マネージャーの更新
	obstacleManager_->Update();
}

void StageEditScene::Draw2D()
{

}

void StageEditScene::Draw3D()
{
	float fieldSize = 300.0f;

	// グリッドの描画
	LineManager::GetInstance()->DrawGrid(
		fieldSize, 
		3.0f, 
		VectorColorCodes::White
	);

	//原点がわかるように球を描画
	LineManager::GetInstance()->DrawSphere(
		Vector3(),
		0.3f,
		VectorColorCodes::Red
	);

	// X軸の線、少し浮かして見やすくする
	LineManager::GetInstance()->DrawLine(
		Vector3(-fieldSize, 0.05f, 0.0f),
		Vector3(fieldSize, 0.05f, 0.0f),
		VectorColorCodes::Red
	);

	// Y軸の線
	LineManager::GetInstance()->DrawLine(
		Vector3(0.0f, -fieldSize, 0.0f),
		Vector3(0.0f, fieldSize, 0.0f),
		VectorColorCodes::Green
	);

	// Z軸の線、少し浮かして見やすくする
	LineManager::GetInstance()->DrawLine(
		Vector3(0.0f, 0.05f, -fieldSize),
		Vector3(0.0f, 0.05f, fieldSize),
		VectorColorCodes::Blue
	);

	// スカイドームの描画
	skydome_->Draw();

	//　障害物の描画
	obstacleManager_->Draw(sceneManager_->GetCameraManager());
}

void StageEditScene::Finalize()
{
}
