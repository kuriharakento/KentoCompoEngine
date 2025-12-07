#include "GameClearScene.h"

#include "engine/scene/manager/SceneManager.h"
#include <input/Input.h>
#include "externals/imgui/imgui.h"
#include "manager/graphics/ShadowMapManager.h"

void GameClearScene::Initialize()
{
	auto* object3dCommon = sceneManager_->GetObject3dCommon();
	auto* cameraManager = sceneManager_->GetCameraManager();
	auto* lightManager = sceneManager_->GetLightManager();

	// 地面オブジェクトの作成
	ground_ = std::make_unique<Object3d>();
	ground_->Initialize(object3dCommon);
	ground_->SetModel("terrain");
	ground_->SetScale({ 1.0f, 1.0f, 1.0f });
	ground_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	ground_->SetLightManager(lightManager);

	// テスト用オブジェクト（キューブ）の作成
	for (int i = 0; i < 3; ++i) {
		auto obj = std::make_unique<Object3d>();
		obj->Initialize(object3dCommon);
		obj->SetModel("cube");
		obj->SetScale({ 1.0f, 1.0f, 1.0f });
		obj->SetTranslate({ static_cast<float>(i - 1) * 3.0f, 5.0f, 0.0f });
		obj->SetLightManager(lightManager);
		testObjects_.push_back(std::move(obj));
	}

	StartState(SceneState::Playing);
}

void GameClearScene::Finalize()
{
	testObjects_.clear();
	ground_.reset();
}

void GameClearScene::Draw3D()
{
	// 地面の描画
	if (ground_) {
		ground_->Draw();
	}

	// テストオブジェクトの描画
	for (auto& obj : testObjects_) {
		obj->Draw();
	}
}

void GameClearScene::Draw2D()
{
}

void GameClearScene::DrawShadow()
{
	auto* lightManager = sceneManager_->GetLightManager();
	auto* shadowMapManager = sceneManager_->GetShadowMapManager();
	
	// 現在のカスケードインデックスに対応する行列を取得
	uint32_t cascadeIndex = shadowMapManager->GetCurrentCascadeIndex();
	const Matrix4x4& cascadeViewProj = lightManager->GetCascadeViewProjection(cascadeIndex);
	
	// 注: 地面はシャドウマップに描画しない（自己シャドウを防ぐため）
	
	// テストオブジェクトのシャドウ描画
	for (auto& obj : testObjects_) {
		obj->DrawShadowWithMatrix(cascadeViewProj);
	}
}



void GameClearScene::OnEnterPlaying()
{
}

void GameClearScene::OnUpdatePlaying()
{
	auto* cameraManager = sceneManager_->GetCameraManager();

	// オブジェクトの回転
	objectRotation_ += 0.01f;
	for (size_t i = 0; i < testObjects_.size(); ++i) {
		testObjects_[i]->SetRotate({ 0.0f, objectRotation_ + static_cast<float>(i) * 0.5f, 0.0f });
		testObjects_[i]->Update(cameraManager);
	}
	if (ground_) {
		ground_->Update(cameraManager);
	}

	// スペースキーでタイトルに戻る
	if (Input::GetInstance()->TriggerKey(DIK_SPACE))
	{
		sceneManager_->ChangeScene(SceneNames::Title);
	}
}

void GameClearScene::DrawImGui()
{
	ImGui::Begin("Shadow Test Scene");

	if (ImGui::CollapsingHeader("Light Settings")) {
		auto* lightManager = sceneManager_->GetLightManager();
		
		// ライト方向の調整
		static float lightDir[3] = { 
			lightManager->GetDirectionalLight().direction.x, 
			lightManager->GetDirectionalLight().direction.y, 
			lightManager->GetDirectionalLight().direction.z 
		};
		
		if (ImGui::DragFloat3("Light Direction", lightDir, 0.01f, -1.0f, 1.0f)) {
			DirectionalLight light = lightManager->GetDirectionalLight();
			light.direction = { lightDir[0], lightDir[1], lightDir[2] };
			lightManager->SetDirectionalLight(light);
		}
	}

	if (ImGui::CollapsingHeader("Objects")) {
		// 地面の操作
		if (ground_) {
			if (ImGui::TreeNode("Ground")) {
				Vector3 scale = ground_->GetScale();
				Vector3 rotate = ground_->GetRotate();
				Vector3 translate = ground_->GetTranslate();

				bool changed = false;
				changed |= ImGui::DragFloat3("Scale", &scale.x, 0.1f);
				changed |= ImGui::DragFloat3("Rotate", &rotate.x, 0.01f);
				changed |= ImGui::DragFloat3("Translate", &translate.x, 0.1f);

				if (changed) {
					ground_->SetScale(scale);
					ground_->SetRotate(rotate);
					ground_->SetTranslate(translate);
				}
				ImGui::TreePop();
			}
		}

		// テストオブジェクトの操作
		for (size_t i = 0; i < testObjects_.size(); ++i) {
			std::string label = "Object " + std::to_string(i);
			if (ImGui::TreeNode(label.c_str())) {
				Vector3 scale = testObjects_[i]->GetScale();
				Vector3 rotate = testObjects_[i]->GetRotate();
				Vector3 translate = testObjects_[i]->GetTranslate();

				bool changed = false;
				changed |= ImGui::DragFloat3("Scale", &scale.x, 0.1f);
				changed |= ImGui::DragFloat3("Rotate", &rotate.x, 0.01f);
				changed |= ImGui::DragFloat3("Translate", &translate.x, 0.1f);

				if (changed) {
					testObjects_[i]->SetScale(scale);
					testObjects_[i]->SetRotate(rotate);
					testObjects_[i]->SetTranslate(translate);
				}
				ImGui::TreePop();
			}
		}
	}

	ImGui::End();
}

void GameClearScene::OnExitPlaying()
{
}
