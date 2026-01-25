#include "GameClearScene.h"

#include "engine/scene/manager/SceneManager.h"
#include <input/Input.h>
#include "externals/imgui/imgui.h"
#include "manager/graphics/ShadowMapManager.h"
#include "manager/effect/PostProcessManager.h"

void GameClearScene::Initialize()
{
	auto* object3dCommon = sceneManager_->GetObject3dCommon();
	auto* cameraManager = sceneManager_->GetCameraManager();
	auto* lightManager = sceneManager_->GetLightManager();

	// ブルームを無効化
	sceneManager_->GetPostProcessManager()->bloomEffect_->SetEnabled(false);

	// 地面オブジェクトの作成
	ground_ = std::make_unique<Object3d>();
	ground_->Initialize(object3dCommon);
	ground_->SetModel("terrain");
	ground_->SetScale({ 1.0f, 1.0f, 1.0f });
	ground_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	ground_->SetTranslate({ 0.0f, 0.0f, 0.0f });
	ground_->SetLightManager(lightManager);
	RegisterObject(ground_.get());

	// テスト用オブジェクト（キューブ）の作成
	for (int i = 0; i < 3; ++i) {
		auto obj = std::make_unique<Object3d>();
		obj->Initialize(object3dCommon);
		if(i == 0){
			obj->SetModel("plane");
		}
		else if(i == 1){
			obj->SetModel("multimesh");
		}
		else{
			obj->SetModel("multimaterial");
		}

		obj->SetScale({ 1.0f, 1.0f, 1.0f });
		obj->SetTranslate({ static_cast<float>(i - 1) * 3.0f, 5.0f, 0.0f });
		obj->SetLightManager(lightManager);
		RegisterObject(obj.get());
		testObjects_.push_back(std::move(obj));
	}

	// スキニングオブジェクトの作成
	skinnedObject_ = std::make_unique<SkinnedObject3d>();
	skinnedObject_->Initialize(object3dCommon);
	skinnedObject_->SetModel("sneakWalk", ".gltf");
	skinnedObject_->SetCamera(cameraManager->GetActiveCamera());
	skinnedObject_->SetLightManager(lightManager);
	skinnedObject_->SetTranslate({ 5.0f, 0.0f, 0.0f }); // 右側に配置
	skinnedObject_->SetScale({ 1.0f, 1.0f, 1.0f });
	//skinnedObject_->PlayAnimation(0, true);

	// デバッグカメラの初期化
	debugCamera_.Initialize(cameraManager->GetActiveCamera());
	debugCamera_.Start({ 0.0f, 10.0f, -20.0f }, { 0.5f, 0.0f, 0.0f });

	StartState(SceneState::Playing);
}

void GameClearScene::Finalize()
{
	ClearObjects();
	testObjects_.clear();
	ground_.reset();
}

void GameClearScene::Draw2D()
{
}

void GameClearScene::Draw3D()
{
	// 基底クラスの描画（登録済みオブジェクト）
	BaseScene::Draw3D();

	// スキニングオブジェクトの描画（DispatchSkinningはUpdateで実行済み）
	if (skinnedObject_) {
		skinnedObject_->Draw();
	}
}

void GameClearScene::DrawGBuffer()
{
	// 基底クラスのG-Buffer描画（登録済みオブジェクト）
	BaseScene::DrawGBuffer();

	// スキニングオブジェクトのG-Buffer描画（DispatchSkinningはUpdateで実行済み）
	if (skinnedObject_) {
		skinnedObject_->DrawGBuffer();
	}
}


void GameClearScene::OnEnterPlaying()

{
}

void GameClearScene::OnUpdatePlaying()
{
	auto* cameraManager = sceneManager_->GetCameraManager();
	float deltaTime = 1.0f / 60.0f; // TODO: TimeManagerから取得

	debugCamera_.Update();

	for (size_t i = 0; i < testObjects_.size(); ++i) {
		testObjects_[i]->Update(cameraManager);
	}
	if (ground_) {
		ground_->Update(cameraManager);
	}

	// スキニングオブジェクトの更新とスキニング計算
	if (skinnedObject_) {
		skinnedObject_->Update(deltaTime, nullptr);
		skinnedObject_->DispatchSkinning(); // ここで一度だけ実行
	}
}

void GameClearScene::DrawShadow()
{
	// 地面のシャドウ描画
	if (ground_) {
		ground_->DrawShadowOnly();
	}

	// テストオブジェクトのシャドウ描画
	for (auto& obj : testObjects_) {
		obj->DrawShadowOnly();
	}

	// スキニングオブジェクトのシャドウ描画（DispatchSkinningはUpdateで実行済み）
	if (skinnedObject_) {
		skinnedObject_->DrawShadowOnly();
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

	// スキニングオブジェクトの設定
	if (ImGui::CollapsingHeader("Skinned Object")) {
		if (skinnedObject_ && skinnedObject_->GetModel()) {
			const auto& animations = skinnedObject_->GetModel()->GetAnimations();
			
			// アニメーション選択コンボボックスの作成
			// -1 = バインドポーズ、0以上 = アニメーション
			std::vector<std::string> animNames;
			animNames.push_back("Bind Pose");
			for (const auto& anim : animations) {
				animNames.push_back(anim.name.empty() ? "Animation " + std::to_string(animNames.size() - 1) : anim.name);
			}
			
			// 現在の選択肢のプレビュー名
			const char* previewName = (selectedAnimationIndex_ < 0) ? 
				"Bind Pose" : animNames[selectedAnimationIndex_ + 1].c_str();
			
			if (ImGui::BeginCombo("Animation", previewName)) {
				for (int i = -1; i < static_cast<int>(animations.size()); ++i) {
					bool isSelected = (selectedAnimationIndex_ == i);
					const char* name = (i < 0) ? "Bind Pose" : animNames[i + 1].c_str();
					
					if (ImGui::Selectable(name, isSelected)) {
						selectedAnimationIndex_ = i;
						if (i < 0) {
							// バインドポーズ
							skinnedObject_->StopAnimation();
						} else {
							// アニメーション再生
							skinnedObject_->PlayAnimation(i, true);
						}
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			// トランスフォーム編集
			Vector3 scale = skinnedObject_->GetScale();
			Vector3 rotate = skinnedObject_->GetRotate();
			Vector3 translate = skinnedObject_->GetTranslate();

			bool changed = false;
			changed |= ImGui::DragFloat3("Scale##Skinned", &scale.x, 0.1f);
			changed |= ImGui::DragFloat3("Rotate##Skinned", &rotate.x, 0.01f);
			changed |= ImGui::DragFloat3("Translate##Skinned", &translate.x, 0.1f);

			if (changed) {
				skinnedObject_->SetScale(scale);
				skinnedObject_->SetRotate(rotate);
				skinnedObject_->SetTranslate(translate);
			}
		}
	}

	ImGui::End();
}

void GameClearScene::OnExitPlaying()
{
}
