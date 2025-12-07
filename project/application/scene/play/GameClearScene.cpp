#include "GameClearScene.h"

#include "engine/scene/manager/SceneManager.h"
#include <input/Input.h>
#include "externals/imgui/imgui.h"

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

void GameClearScene::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Shadow Test");
	ImGui::Text("Press SPACE to return to title");
	ImGui::Text("Object rotation: %.2f", objectRotation_);
	ImGui::End();
#endif
}

void GameClearScene::DrawShadow()
{
	auto* lightManager = sceneManager_->GetLightManager();
	D3D12_GPU_VIRTUAL_ADDRESS shadowMatrixAddr = lightManager->GetShadowMatrixGPUAddress();

	// 地面のシャドウ描画
	if (ground_) {
		ground_->DrawShadow(shadowMatrixAddr);
	}

	// テストオブジェクトのシャドウ描画
	for (auto& obj : testObjects_) {
		obj->DrawShadow(shadowMatrixAddr);
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

void GameClearScene::OnExitPlaying()
{
}
