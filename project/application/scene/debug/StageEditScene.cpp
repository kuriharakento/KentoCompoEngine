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
	// Playing状態から開始
	StartState(SceneState::Playing);

	// ステージマネージャーの初期化
	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetCameraManager()
	);
	// デフォルトステージをロード
	stageManager_->LoadStage("field");

	// デバッグカメラの初期化（自由視点での編集を可能にする）
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(
		sceneManager_->GetCameraManager()->GetActiveCamera()
	);
	debugCamera_->Start();
}

void StageEditScene::Finalize()
{
	// リソース解放処理（現状は特になし）
}

// ==================================================
// 状態フック
// ==================================================

void StageEditScene::OnUpdatePlaying()
{
#ifdef USE_IMGUI
	// ステージ編集用のImGuiインターフェース
	ImGui::Begin("StageEditScene");
	static std::string stageName = "field";
	static char stageNameBuffer[128] = "field";
	
	// ステージ名入力フィールド
	if (ImGui::InputText("Stage Name", stageNameBuffer, sizeof(stageNameBuffer)))
	{
		stageName = stageNameBuffer;
	}
	
	// ステージロードボタン
	if (ImGui::Button("Load Stage"))
	{
		stageManager_->LoadStage(stageName);
	}
	ImGui::End();
#endif

	// デバッグカメラの更新（WASD移動、マウス視点変更）
	if (debugCamera_) debugCamera_->Update();

	// ステージ内オブジェクトの更新
	if (stageManager_) stageManager_->Update();
}

// ==================================================
// 描画処理
// ==================================================

void StageEditScene::Draw2D()
{
	// 2D要素の描画（現状は特になし）
}

void StageEditScene::Draw3D()
{
	float fieldSize = 300.0f;

	// グリッド描画（空間の把握を補助）
	LineManager::GetInstance()->DrawGrid(
		fieldSize,
		3.0f,
		VectorColorCodes::White
	);

	// 原点の可視化（赤い球）
	LineManager::GetInstance()->DrawSphere(
		Vector3(),
		0.3f,
		VectorColorCodes::Red
	);

	// X軸の可視化（赤線）
	LineManager::GetInstance()->DrawLine(
		Vector3(-fieldSize, 0.05f, 0.0f),
		Vector3(fieldSize, 0.05f, 0.0f),
		VectorColorCodes::Red
	);

	// Y軸の可視化（緑線）
	LineManager::GetInstance()->DrawLine(
		Vector3(0.0f, -fieldSize, 0.0f),
		Vector3(0.0f, fieldSize, 0.0f),
		VectorColorCodes::Green
	);

	// Z軸の可視化（青線）
	LineManager::GetInstance()->DrawLine(
		Vector3(0.0f, 0.05f, -fieldSize),
		Vector3(0.0f, 0.05f, fieldSize),
		VectorColorCodes::Blue
	);

	// ステージオブジェクトの描画
	if (stageManager_) stageManager_->Draw();
}