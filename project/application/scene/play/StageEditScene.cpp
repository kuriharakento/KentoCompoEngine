#include "StageEditScene.h"

// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"
// scene
#include "scene/manager/SceneManager.h"

void StageEditScene::Initialize()
{
	// 障害物マネージャーの初期化
	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetLightManager()
	);
	stageManager_->LoadStage("field");

	//デバッグカメラの初期化
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(
		sceneManager_->GetCameraManager()->GetActiveCamera()
	);
	debugCamera_->Start();
}

void StageEditScene::Update()
{
#ifdef _DEBUG
	// ImGuiの描画
	ImGui::Begin("StageEditScene");
	if (ImGui::Button("Load Stage"))
	{
		stageManager_->LoadStage("field");
	}
	ImGui::End();
#endif

	// デバッグカメラの更新
	debugCamera_->Update();

	// ステージマネージャーの更新
	stageManager_->Update();
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

	// ステージマネージャーの描画
	stageManager_->Draw(sceneManager_->GetCameraManager());
}

void StageEditScene::Finalize()
{
}
