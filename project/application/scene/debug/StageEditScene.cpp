#include "StageEditScene.h"

// system
#include "manager/graphics/LineManager.h"
// math
#include "math/VectorColorCodes.h"
// scene
#include "scene/manager/SceneManager.h"
#include "externals/imgui/imgui.h"
#ifdef USE_IMGUI
#include "manager/editor/DebugUIManager.h"
#endif

void StageEditScene::Initialize()
{
	StartState(SceneState::Playing);

	stageManager_ = std::make_unique<StageManager>();
	stageManager_->Initialize(
		nullptr, // registry
		nullptr, // systemManager
		sceneManager_->GetObject3dCommon(),
		sceneManager_->GetSpriteCommon(),
		sceneManager_->GetLightManager(),
		sceneManager_->GetCameraManager(),
		sceneManager_->GetShadowMapManager()
	);
	stageManager_->LoadStage("field");

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize(
		sceneManager_->GetCameraManager()->GetActiveCamera()
	);
	debugCamera_->Start();

#ifdef USE_IMGUI
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "StageEditScene", [this]() { this->DrawImGui(); }, DebugUIArea::Hierarchy);
#endif
}

void StageEditScene::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance()) {
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
}

// ==================================================
// 状態フック
// ==================================================

void StageEditScene::DrawImGui()
{
#ifdef USE_IMGUI
	static std::string stageName = "field";
	static char stageNameBuffer[128] = "field";
	
	if (ImGui::InputText("Stage Name", stageNameBuffer, sizeof(stageNameBuffer)))
	{
		stageName = stageNameBuffer;
	}
	
	if (ImGui::Button("Load Stage"))
	{
		stageManager_->LoadStage(stageName);
	}
#endif
}

void StageEditScene::OnUpdatePlaying()
{
	if (debugCamera_) debugCamera_->Update();

	if (stageManager_) stageManager_->Update();
}

// ==================================================
// 描画処理
// ==================================================

void StageEditScene::Draw2D()
{
	if (stageManager_) stageManager_->Draw2D();
}

void StageEditScene::Draw3D()
{
	float fieldSize = 300.0f;

	LineManager::GetInstance()->DrawGrid(
		fieldSize,
		3.0f,
		VectorColorCodes::White
	);

	LineManager::GetInstance()->DrawSphere(
		Vector3(),
		0.3f,
		VectorColorCodes::Red
	);

	LineManager::GetInstance()->DrawLine(
		Vector3(-fieldSize, 0.05f, 0.0f),
		Vector3(fieldSize, 0.05f, 0.0f),
		VectorColorCodes::Red
	);

	LineManager::GetInstance()->DrawLine(
		Vector3(0.0f, -fieldSize, 0.0f),
		Vector3(0.0f, fieldSize, 0.0f),
		VectorColorCodes::Green
	);

	LineManager::GetInstance()->DrawLine(
		Vector3(0.0f, 0.05f, -fieldSize),
		Vector3(0.0f, 0.05f, fieldSize),
		VectorColorCodes::Blue
	);

	if (stageManager_) stageManager_->Draw3D();
}