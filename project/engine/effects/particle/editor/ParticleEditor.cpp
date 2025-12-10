#include "ParticleEditor.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/renderer/RibbonRenderer.h"
#include "effects/particle/renderer/MeshRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/module/update/TextureSheetModule.h"
#include "effects/particle/module/update/ForceFieldModules.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/graphics/TextureManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "math/BlendMode.h"
#include "time/TimeManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

ParticleEditor::ParticleEditor() = default;
ParticleEditor::~ParticleEditor()
{
	if (currentEffect_)
	{
		ParticleManager::GetInstance()->RemoveEffect(currentEffect_);
	}
}

void ParticleEditor::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	NewEffect();
}

void ParticleEditor::Update(CameraManager* camera)
{
#ifdef USE_IMGUI
	if (!isVisible_) return;

	// エフェクトの更新はParticleManagerが行うため、ここでは呼び出さない
	/*
	if (currentEffect_)
	{
		float deltaTime = TimeManager::GetInstance().GetGameContext().deltaTime;
		currentEffect_->Update(deltaTime, camera);
	}
	*/

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Particle Editor", &isVisible_, ImGuiWindowFlags_MenuBar);

	DrawMenuBar();

	// 左右分割
	ImGui::Columns(2, "EditorColumns");
	ImGui::SetColumnWidth(0, 300);

	// 左側：エフェクト/エミッター設定
	DrawEffectPanel();
	DrawEmitterPanel();

	ImGui::NextColumn();

	// 右側：モジュール/レンダラー設定
	DrawModulePanel();
	DrawRendererPanel();

	ImGui::Columns(1);
	ImGui::End();

	// ダイアログ
	if (showAddEmitterDialog_)
	{
		AddEmitterDialog();
	}
	if (showAddModuleDialog_ && selectedEmitterIndex_ >= 0)
	{
		AddModuleDialog(currentEffect_->GetEmitter(static_cast<size_t>(selectedEmitterIndex_)));
	}
#else
	(void)camera;
#endif
}

void ParticleEditor::Draw()
{
	// 描画はParticleManagerが行うため、ここでは何もしない
}

void ParticleEditor::NewEffect()
{
	if (currentEffect_)
	{
		ParticleManager::GetInstance()->RemoveEffect(currentEffect_);
		currentEffect_ = nullptr;
	}

	auto effect = std::make_unique<ParticleEffect>();
	effect->Initialize("NewEffect");
	effect->Play();
	
	// エディタ用なので自動削除しない
	effect->SetAutoRemove(false);

	// Managerに登録し、ポインタを保持
	currentEffect_ = effect.get();
	ParticleManager::GetInstance()->AddEffect(std::move(effect));

	selectedEmitterIndex_ = -1;
	selectedModuleIndex_ = -1;
	effectPath_.clear();
	strcpy_s(effectNameBuffer_, "NewEffect");
}

void ParticleEditor::LoadEffect(const std::string& path)
{
	auto effect = ParticleEffect::LoadFromFile(path);
	if (effect)
	{
		if (currentEffect_)
		{
			ParticleManager::GetInstance()->RemoveEffect(currentEffect_);
		}

		// エディタ用なので自動削除しない
		effect->SetAutoRemove(false);
		effectPath_ = path;
		strcpy_s(effectNameBuffer_, effect->GetName().c_str());
		effect->Play();

		currentEffect_ = effect.get();
		ParticleManager::GetInstance()->AddEffect(std::move(effect));
	}
}

void ParticleEditor::SaveEffect(const std::string& path)
{
	if (currentEffect_)
	{
		currentEffect_->SaveToFile(path);
		effectPath_ = path;
	}
}

#ifdef USE_IMGUI
void ParticleEditor::DrawMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New")) { NewEffect(); }
			if (ImGui::MenuItem("Save")) { if (!effectPath_.empty()) SaveEffect(effectPath_); }
			ImGui::Separator();
			if (ImGui::MenuItem("Close")) { isVisible_ = false; }
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Add Emitter")) { showAddEmitterDialog_ = true; }
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("Reset View")) { if (currentEffect_) { currentEffect_->Reset(); currentEffect_->Play(); } }
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void ParticleEditor::DrawEffectPanel()
{
	if (ImGui::CollapsingHeader("Effect", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Columns(2, "EffectParams", false);
		ImGui::SetColumnWidth(0, 100);

		ImGui::Text("Name"); ImGui::NextColumn();
		ImGui::InputText("##Name", effectNameBuffer_, sizeof(effectNameBuffer_)); ImGui::NextColumn();

		if (currentEffect_)
		{
			Vector3 pos = currentEffect_->GetPosition();
			ImGui::Text("Position"); ImGui::NextColumn();
			if (ImGui::DragFloat3("##Position", &pos.x, 0.1f)) { currentEffect_->SetPosition(pos); }
			ImGui::NextColumn();

			ImGui::Text("Status"); ImGui::NextColumn();
			bool isPlaying = currentEffect_->IsPlaying();
			if (ImGui::Checkbox("Playing", &isPlaying))
			{
				if (isPlaying) currentEffect_->Play();
				else currentEffect_->Stop();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset")) { currentEffect_->Reset(); currentEffect_->Play(); }
			ImGui::NextColumn();

			ImGui::Text("Emitters"); ImGui::NextColumn();
			ImGui::Text("%d", static_cast<int>(currentEffect_->GetEmitterCount())); ImGui::NextColumn();
		}
		ImGui::Columns(1);
	}
}

void ParticleEditor::DrawEmitterPanel()
{
	if (!currentEffect_) return;

	if (ImGui::CollapsingHeader("Emitters", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
		{
			auto* emitter = currentEffect_->GetEmitter(i);
			bool isSelected = (selectedEmitterIndex_ == static_cast<int>(i));

			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Selectable(emitter->GetName().c_str(), isSelected))
			{
				selectedEmitterIndex_ = static_cast<int>(i);
				selectedModuleIndex_ = -1;
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		if (ImGui::Button("+ Add Emitter")) { showAddEmitterDialog_ = true; }
	}
}

void ParticleEditor::DrawModulePanel()
{
	if (!currentEffect_ || selectedEmitterIndex_ < 0) return;

	auto* emitter = currentEffect_->GetEmitter(static_cast<size_t>(selectedEmitterIndex_));
	if (!emitter) return;

	// エミッター基本設定
	if (ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int maxParticles = static_cast<int>(emitter->GetMaxParticles());
		if (ImGui::InputInt("Max Particles", &maxParticles))
		{
			emitter->SetMaxParticles(static_cast<uint32_t>((std::max)(1, maxParticles)));
		}

		Vector3 pos = emitter->GetPosition();
		if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
		{
			emitter->SetPosition(pos);
		}

		// シミュレーションモード
		const char* simModes[] = { "CPU", "GPU" };
		int currentMode = static_cast<int>(emitter->GetSimulationMode());
		if (ImGui::Combo("Simulation", &currentMode, simModes, 2))
		{
			emitter->SetSimulationMode(static_cast<SimulationMode>(currentMode));
		}

		ImGui::Text("Active Particles: %d", static_cast<int>(emitter->GetParticles().size()));
	}

	// モジュールリスト
	if (ImGui::CollapsingHeader("Modules", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (size_t i = 0; i < emitter->GetModuleCount(); ++i)
		{
			auto* module = emitter->GetModule(i);
			if (!module) continue;

			ImGui::PushID(static_cast<int>(i));

			bool isSelected = (selectedModuleIndex_ == static_cast<int>(i));
			const char* phaseName = module->GetPhase() == ModulePhase::Spawn ? "[Spawn]" : "[Update]";

			char label[256];
			snprintf(label, sizeof(label), "%s %s", phaseName, module->GetName());

			if (ImGui::Selectable(label, isSelected))
			{
				selectedModuleIndex_ = static_cast<int>(i);
			}

			// 右クリックコンテキストメニュー
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Move Up") && i > 0) { emitter->MoveModuleUp(i); }
				if (ImGui::MenuItem("Move Down") && i < emitter->GetModuleCount() - 1) { emitter->MoveModuleDown(i); }
				ImGui::Separator();
				if (ImGui::MenuItem("Delete")) { emitter->RemoveModule(i); selectedModuleIndex_ = -1; }
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		ImGui::Separator();
		if (ImGui::Button("+ Add Module")) { showAddModuleDialog_ = true; }

		// 選択中のモジュールのプロパティ編集
		if (selectedModuleIndex_ >= 0)
		{
			auto* selectedModule = emitter->GetModule(static_cast<size_t>(selectedModuleIndex_));
			if (selectedModule)
			{
				ImGui::Separator();
				DrawModuleProperties(selectedModule);
			}
		}
	}
}

void ParticleEditor::DrawModuleProperties(IModule* module)
{
	if (!module) return;

	ImGui::Text("Module: %s", module->GetName());
	ImGui::Separator();

	// 各モジュールタイプごとにキャスト
	if (auto* m = dynamic_cast<SpawnRateModule*>(module))
	{
		float rate = m->GetRate();
		if (ImGui::DragFloat("Rate (per sec)", &rate, 0.1f, 0.0f, 1000.0f))
		{
			m->SetRate(rate);
		}
	}
	else if (auto* m = dynamic_cast<SpawnBurstModule*>(module))
	{
		int count = static_cast<int>(m->GetCount());
		if (ImGui::InputInt("Count", &count)) { m->SetCount(static_cast<uint32_t>((std::max)(0, count))); }

		float delay = m->GetDelay();
		if (ImGui::DragFloat("Delay", &delay, 0.01f, 0.0f, 10.0f)) { m->SetDelay(delay); }

		float interval = m->GetInterval();
		if (ImGui::DragFloat("Interval", &interval, 0.01f, 0.0f, 10.0f)) { m->SetInterval(interval); }

		int loops = m->GetLoops();
		if (ImGui::InputInt("Loops (-1=infinite)", &loops)) { m->SetLoops(loops); }
	}
	else if (auto* m = dynamic_cast<InitialLifetimeModule*>(module))
	{
		float min = m->GetMinLifetime();
		float max = m->GetMaxLifetime();
		if (ImGui::DragFloat("Min Lifetime", &min, 0.01f, 0.01f, 100.0f)) { m->SetMinLifetime(min); }
		if (ImGui::DragFloat("Max Lifetime", &max, 0.01f, 0.01f, 100.0f)) { m->SetMaxLifetime(max); }
	}
	else if (auto* m = dynamic_cast<InitialVelocityModule*>(module))
	{
		Vector3 min = m->GetMinVelocity();
		Vector3 max = m->GetMaxVelocity();
		if (ImGui::DragFloat3("Min Velocity", &min.x, 0.1f)) { m->SetMinVelocity(min); }
		if (ImGui::DragFloat3("Max Velocity", &max.x, 0.1f)) { m->SetMaxVelocity(max); }
	}
	else if (auto* m = dynamic_cast<InitialScaleModule*>(module))
	{
		Vector3 min = m->GetMinScale();
		Vector3 max = m->GetMaxScale();
		if (ImGui::DragFloat3("Min Scale", &min.x, 0.01f)) { m->SetMinScale(min); }
		if (ImGui::DragFloat3("Max Scale", &max.x, 0.01f)) { m->SetMaxScale(max); }
	}
	else if (auto* m = dynamic_cast<InitialColorModule*>(module))
	{
		Vector4 min = m->GetMinColor();
		Vector4 max = m->GetMaxColor();
		if (ImGui::ColorEdit4("Min Color", &min.x)) { m->SetMinColor(min); }
		if (ImGui::ColorEdit4("Max Color", &max.x)) { m->SetMaxColor(max); }
	}
	else if (auto* m = dynamic_cast<GravityModule*>(module))
	{
		Vector3 g = m->GetGravity();
		if (ImGui::DragFloat3("Gravity", &g.x, 0.1f)) { m->SetGravity(g); }
	}
	else if (auto* m = dynamic_cast<DragModule*>(module))
	{
		float drag = m->GetDrag();
		if (ImGui::DragFloat("Drag", &drag, 0.01f, 0.0f, 10.0f)) { m->SetDrag(drag); }
	}
	else if (auto* m = dynamic_cast<ColorFadeModule*>(module))
	{
		Vector4 start = m->GetStartColor();
		Vector4 end = m->GetEndColor();
		if (ImGui::ColorEdit4("Start Color", &start.x)) { m->SetStartColor(start); }
		if (ImGui::ColorEdit4("End Color", &end.x)) { m->SetEndColor(end); }
	}
	else if (auto* m = dynamic_cast<ScaleOverLifetimeModule*>(module))
	{
		Vector3 start = m->GetStartScale();
		Vector3 end = m->GetEndScale();
		if (ImGui::DragFloat3("Start Scale", &start.x, 0.01f)) { m->SetStartScale(start); }
		if (ImGui::DragFloat3("End Scale", &end.x, 0.01f)) { m->SetEndScale(end); }
	}
	else if (auto* m = dynamic_cast<TextureSheetModule*>(module))
	{
		int cols = static_cast<int>(m->GetColumns());
		int rows = static_cast<int>(m->GetRows());
		if (ImGui::InputInt("Columns", &cols)) { m->SetGridSize(static_cast<uint32_t>((std::max)(1, cols)), m->GetRows()); }
		if (ImGui::InputInt("Rows", &rows)) { m->SetGridSize(m->GetColumns(), static_cast<uint32_t>((std::max)(1, rows))); }

		float fps = m->GetFrameRate();
		if (ImGui::DragFloat("Frame Rate", &fps, 0.1f, 0.1f, 120.0f)) { m->SetFrameRate(fps); }

		const char* playModes[] = { "Loop", "Once", "PingPong" };
		int mode = static_cast<int>(m->GetPlayMode());
		if (ImGui::Combo("Play Mode", &mode, playModes, 3)) { m->SetPlayMode(static_cast<TextureSheetPlayMode>(mode)); }
	}
	else if (auto* m = dynamic_cast<AttractorModule*>(module))
	{
		Vector3 target = m->GetTarget();
		if (ImGui::DragFloat3("Target", &target.x, 0.1f)) { m->SetTarget(target); }

		float strength = m->GetStrength();
		if (ImGui::DragFloat("Strength", &strength, 0.1f)) { m->SetStrength(strength); }

		float range = m->GetRange();
		if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 100.0f)) { m->SetRange(range); }

		const char* falloffTypes[] = { "None", "Linear", "Inverse Square" };
		int falloff = static_cast<int>(m->GetFalloffType());
		if (ImGui::Combo("Falloff", &falloff, falloffTypes, 3)) { m->SetFalloffType(static_cast<FalloffType>(falloff)); }
	}
	else if (auto* m = dynamic_cast<VortexModule*>(module))
	{
		Vector3 axis = m->GetAxis();
		Vector3 center = m->GetCenter();
		if (ImGui::DragFloat3("Axis", &axis.x, 0.01f)) { m->SetAxis(axis); }
		if (ImGui::DragFloat3("Center", &center.x, 0.1f)) { m->SetCenter(center); }

		float strength = m->GetStrength();
		if (ImGui::DragFloat("Strength", &strength, 0.1f)) { m->SetStrength(strength); }

		float range = m->GetRange();
		if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 100.0f)) { m->SetRange(range); }
	}
	else
	{
		ImGui::TextDisabled("(No properties available)");
	}
}

void ParticleEditor::DrawRendererPanel()
{
	if (!currentEffect_ || selectedEmitterIndex_ < 0) return;

	auto* emitter = currentEffect_->GetEmitter(static_cast<size_t>(selectedEmitterIndex_));
	if (!emitter) return;

	if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto* renderer = emitter->GetRenderer();
		if (renderer)
		{
			const char* typeNames[] = { "Sprite", "Ribbon", "Mesh" };
			int currentType = static_cast<int>(renderer->GetType());
			if (ImGui::Combo("Type", &currentType, typeNames, 3))
			{
				RendererType newType = static_cast<RendererType>(currentType);
				if (newType != renderer->GetType())
				{
					std::unique_ptr<IRenderer> newRenderer;
					// std::unique_ptr<IRenderer> newRenderer; // No longer needed here
					// std::string defaultTex = "Resources/uvChecker.png"; // No longer needed

					if (newType == RendererType::Sprite)
					{
						auto newRenderer = std::make_unique<SpriteRenderer>();
						newRenderer->Initialize("Resources/images/particle.png");
						newRenderer->SetBlendMode(renderer->GetBlendMode());
						emitter->SetRenderer(std::move(newRenderer));
						renderer = emitter->GetRenderer();
					}
					else if (newType == RendererType::Ribbon)
					{
						auto newRenderer = std::make_unique<RibbonRenderer>();
						newRenderer->Initialize("Resources/images/particle.png");
						newRenderer->SetBlendMode(renderer->GetBlendMode());
						emitter->SetRenderer(std::move(newRenderer));
						renderer = emitter->GetRenderer();
					}
					else if (newType == RendererType::Mesh)
					{
						auto newRenderer = std::make_unique<MeshRenderer>();
						newRenderer->Initialize("Resources/images/particle.png");
						newRenderer->SetBlendMode(renderer->GetBlendMode());
						emitter->SetRenderer(std::move(newRenderer));
						renderer = emitter->GetRenderer();
					}
				}
			}

			const char* blendModes[] = { "None", "Alpha", "Additive", "Multiply" };
			int currentBlend = static_cast<int>(renderer->GetBlendMode());
			if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, 4))
			{
				renderer->SetBlendMode(static_cast<BlendMode>(currentBlend));
			}

			// Sprite Renderer特有の設定
			if (auto* spriteRenderer = dynamic_cast<SpriteRenderer*>(renderer))
			{
				// TextureManagerから読み込み済みテクスチャを取得
				auto texturePaths = TextureManager::GetInstance()->GetLoadedTexturePaths();

				if (!texturePaths.empty())
				{
					ImGui::Separator();
					ImGui::Text("Texture:");

					// 現在のテクスチャを取得（表示用）
					static int selectedTextureIdx = 0;

					if (ImGui::BeginCombo("##Texture", texturePaths.empty() ? "(None)" : texturePaths[selectedTextureIdx].c_str()))
					{
						for (int i = 0; i < static_cast<int>(texturePaths.size()); ++i)
						{
							bool isSelected = (selectedTextureIdx == i);

							// ファイル名だけ表示（パスが長い場合）
							std::string displayName = texturePaths[i];
							size_t lastSlash = displayName.find_last_of("/\\");
							if (lastSlash != std::string::npos)
							{
								displayName = displayName.substr(lastSlash + 1);
							}
							if (displayName.empty()) displayName = "Unknown";

							// IDの衝突を避けるためにインデックスを付与
							std::string label = displayName + "##" + std::to_string(i);

							if (ImGui::Selectable(label.c_str(), isSelected))
							{
								selectedTextureIdx = i;
								spriteRenderer->SetTexture(texturePaths[i]);
							}

							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
				}
			}

			// Mesh Renderer特有の設定
			if (auto* meshRenderer = dynamic_cast<MeshRenderer*>(renderer))
			{
				const char* primitiveTypes[] = { "Plane", "Ring", "Cylinder", "Sphere", "Torus", "Star", "Heart", "Spiral", "Cone", "Cube" };
				int primType = static_cast<int>(meshRenderer->GetPrimitiveType());
				if (ImGui::Combo("Primitive", &primType, primitiveTypes, 10))
				{
					meshRenderer->SetPrimitive(static_cast<PrimitiveType>(primType));
				}

				float scale = meshRenderer->GetScale();
				if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.01f, 10.0f))
				{
					meshRenderer->SetScale(scale);
				}
			}
		}
		else
		{
			ImGui::Text("No renderer assigned");

			if (ImGui::Button("Create Sprite Renderer"))
			{
				auto newRenderer = std::make_unique<SpriteRenderer>();
				newRenderer->Initialize("Resources/images/particle.png");
				emitter->SetRenderer(std::move(newRenderer));
			}
		}
	}
}

void ParticleEditor::DrawPreviewPanel() { /* Integrated into main view */ }
void ParticleEditor::DrawCurveEditor() { /* TODO: Advanced curve editing */ }
void ParticleEditor::DrawGradientEditor() { /* TODO: Gradient color editing */ }

void ParticleEditor::AddEmitterDialog()
{
	ImGui::OpenPopup("Add Emitter");

	if (ImGui::BeginPopupModal("Add Emitter", &showAddEmitterDialog_, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Emitter Name", emitterNameBuffer_, sizeof(emitterNameBuffer_));

		if (ImGui::Button("Create"))
		{
			auto emitter = std::make_unique<ParticleEmitter>();
			emitter->Initialize(emitterNameBuffer_);

			auto renderer = std::make_unique<SpriteRenderer>();
			renderer->Initialize("Resources/uvChecker.png");
			emitter->SetRenderer(std::move(renderer));

			emitter->AddModule(std::make_unique<SpawnRateModule>(10.0f));
			emitter->AddModule(std::make_unique<InitialLifetimeModule>(1.0f, 2.0f));
			emitter->AddModule(std::make_unique<InitialVelocityModule>(Vector3{ -1, 1, -1 }, Vector3{ 1, 3, 1 }));
			emitter->AddModule(std::make_unique<GravityModule>());
			emitter->AddModule(std::make_unique<ColorFadeModule>());

			currentEffect_->AddEmitter(std::move(emitter));

			showAddEmitterDialog_ = false;
			emitterNameBuffer_[0] = '\0';
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { showAddEmitterDialog_ = false; }

		ImGui::EndPopup();
	}
}

void ParticleEditor::AddModuleDialog(ParticleEmitter* emitter)
{
	if (!emitter) return;

	ImGui::OpenPopup("Add Module");

	if (ImGui::BeginPopupModal("Add Module", &showAddModuleDialog_, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const char* moduleTypes[] = {
			"Spawn Rate", "Spawn Burst",
			"Initial Lifetime", "Initial Velocity", "Initial Scale", "Initial Color",
			"Gravity", "Drag", "Color Fade", "Scale Over Lifetime",
			"Texture Sheet", "Attractor", "Vortex"
		};

		static int selectedModule = 0;
		ImGui::Combo("Module Type", &selectedModule, moduleTypes, IM_ARRAYSIZE(moduleTypes));

		if (ImGui::Button("Add"))
		{
			switch (selectedModule)
			{
			case 0: emitter->AddModule(std::make_unique<SpawnRateModule>()); break;
			case 1: emitter->AddModule(std::make_unique<SpawnBurstModule>()); break;
			case 2: emitter->AddModule(std::make_unique<InitialLifetimeModule>()); break;
			case 3: emitter->AddModule(std::make_unique<InitialVelocityModule>()); break;
			case 4: emitter->AddModule(std::make_unique<InitialScaleModule>()); break;
			case 5: emitter->AddModule(std::make_unique<InitialColorModule>()); break;
			case 6: emitter->AddModule(std::make_unique<GravityModule>()); break;
			case 7: emitter->AddModule(std::make_unique<DragModule>()); break;
			case 8: emitter->AddModule(std::make_unique<ColorFadeModule>()); break;
			case 9: emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>()); break;
			case 10: emitter->AddModule(std::make_unique<TextureSheetModule>()); break;
			case 11: emitter->AddModule(std::make_unique<AttractorModule>()); break;
			case 12: emitter->AddModule(std::make_unique<VortexModule>()); break;
			}
			showAddModuleDialog_ = false;
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { showAddModuleDialog_ = false; }

		ImGui::EndPopup();
	}
}
#endif
