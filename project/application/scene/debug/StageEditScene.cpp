#include "StageEditScene.h"

// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"
// scene
#include "scene/manager/SceneManager.h"
#include "externals/imgui/imgui.h"

void StageEditScene::Initialize()
{
	// 初期状態は Enter（必要な初期化は OnEnterEnter で行う／またはここで行う）
	StartState(SceneState::Playing);

	// 障害物マネージャーの初期化（生成と基本設定）
	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetCameraManager()
	);
	stageManager_->LoadStage("field");

	// デバッグカメラの初期化（Start は OnEnterEnter で行っても良い）
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(
		sceneManager_->GetCameraManager()->GetActiveCamera()
	);
	debugCamera_->Start();
}

void StageEditScene::Finalize()
{
	// 必要なら解放処理
}

// ----------------------------------------------------------------
// フック実装
// ----------------------------------------------------------------

void StageEditScene::OnUpdatePlaying()
{
#ifdef _DEBUG
	// ImGuiの描画
	ImGui::Begin("StageEditScene");
	static std::string stageName = "field";
	static char stageNameBuffer[128] = "field";
	// ステージ読み込み
	if (ImGui::InputText("S", stageNameBuffer, sizeof(stageNameBuffer)))
	{
		stageName = stageNameBuffer;
	}
	if (ImGui::Button("Load Stage"))
	{
		stageManager_->LoadStage(stageName);
	}
	ImGui::End();
#endif

	// デバッグカメラの更新
	if (debugCamera_) debugCamera_->Update();

	// ステージマネージャーの更新
	if (stageManager_) stageManager_->Update();
}

// ----------------------------------------------------------------
// 描画
// ----------------------------------------------------------------

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

	// 原点がわかるように球を描画
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
	if (stageManager_) stageManager_->Draw();
}