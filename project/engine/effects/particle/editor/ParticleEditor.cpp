#include "ParticleEditor.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/ParticleManager.h"
#include "effects/particle/renderer/SpriteRenderer.h"
#include "effects/particle/renderer/TrailRenderer.h"
#include "effects/particle/renderer/MeshRenderer.h"
#include "effects/particle/module/spawn/SpawnModules.h"
#include "effects/particle/module/spawn/InitialModules.h"
#include "effects/particle/module/spawn/SpawnShapeModules.h"
#include "effects/particle/module/update/UpdateModules.h"
#include "effects/particle/module/update/TextureSheetModule.h"
#include "effects/particle/module/update/ForceFieldModules.h"
#include "effects/particle/module/update/RibbonModules.h"
#include "effects/particle/module/update/AdvancedModules.h"
#include "effects/particle/module/update/BehaviorModules.h"
#include "effects/particle/module/update/TrailModule.h"
#include "effects/particle/module/spawn/SubEmitterModule.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/graphics/TextureManager.h"
#include "manager/graphics/LineManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "math/BlendMode.h"
#include "time/TimeManager.h"
#include <filesystem>
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace
{
	// デバッグ表示用のサイズ定数
	constexpr float kAxisSize = 0.5f;
	constexpr float kMarkerSize = 0.3f;
	constexpr float kSmallMarkerSize = 0.15f;
	constexpr float kParticleMarkerSize = 0.05f;
	
	// デバッグ線描画のセグメント数
	constexpr int kCircleSegments = 16;
	constexpr int kConeSegments = 8;
	
	// デフォルトウィンドウサイズ
	constexpr float kDefaultWindowWidth = 400.0f;
	constexpr float kDefaultWindowHeight = 800.0f;
	
	// 数学定数
	constexpr float kPi = 3.14159f;
	constexpr float kTwoPi = 2.0f * kPi;
}

ParticleEditor::ParticleEditor() = default;
ParticleEditor::~ParticleEditor()
{
	// 現在編集中のエフェクトをマネージャーから削除
	if (currentEffect_)
	{
		ParticleManager::GetInstance()->RemoveEffect(currentEffect_);
	}
}

void ParticleEditor::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	
	// 初期エフェクトを作成
	NewEffect();
}

void ParticleEditor::Update(CameraManager* camera)
{
#ifdef USE_IMGUI
	// 非表示時は処理をスキップ
	if (!isVisible_) return;

	// ウィンドウサイズを初回のみ設定
	ImGui::SetNextWindowSize(ImVec2(kDefaultWindowWidth, kDefaultWindowHeight), ImGuiCond_FirstUseEver);
	ImGui::Begin("Particle Editor", &isVisible_, ImGuiWindowFlags_MenuBar);

	// メニューバー描画
	DrawMenuBar();

	// エフェクト全体のパネル
	DrawEffectPanel();
	DrawEmitterPanel();
	
	// エミッター選択時のみモジュールとレンダラーを表示
	if (selectedEmitterIndex_ >= 0)
	{
		DrawModulePanel();
		DrawRendererPanel();
	}

	ImGui::End();

	// エミッター追加ダイアログ
	if (showAddEmitterDialog_)
	{
		AddEmitterDialog();
	}
	
	// モジュール追加ダイアログ
	if (showAddModuleDialog_ && selectedEmitterIndex_ >= 0)
	{
		AddModuleDialog(currentEffect_->GetEmitter(static_cast<size_t>(selectedEmitterIndex_)));
	}
#else
	(void)camera;
#endif
}

void ParticleEditor::DrawDebug()
{
#ifdef USE_IMGUI
	if (!isVisible_ || !showDebug_ || !currentEffect_) return;

	auto* lineManager = LineManager::GetInstance();
	if (!lineManager) return;

	// エミッター位置を表示（黄色の座標軸）
	for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
	{
		auto* emitter = currentEffect_->GetEmitter(i);
		if (!emitter) continue;

		Vector3 emitterPos = emitter->GetPosition();
		
		// エミッター位置に座標軸を表示
		lineManager->DrawAxis(emitterPos, kAxisSize);

		// モジュールごとのデバッグ表示
		for (size_t m = 0; m < emitter->GetModuleCount(); ++m)
		{
			const auto* module = emitter->GetModule(m);
			if (!module) continue;

			// SpawnShape - 形状を可視化
			if (auto* shape = dynamic_cast<const SpawnShapeModule*>(module))
			{
				Vector4 shapeColor = { 0.0f, 1.0f, 0.0f, 1.0f }; // 緑
				SpawnShapeType type = shape->GetShapeType();
				float outerR = shape->GetOuterRadius();
				float innerR = shape->GetInnerRadius();

				// Point - ダイアモンドマーカー
				if (type == SpawnShapeType::Point)
				{
					lineManager->DrawLine({ emitterPos.x - kMarkerSize, emitterPos.y, emitterPos.z }, { emitterPos.x + kMarkerSize, emitterPos.y, emitterPos.z }, shapeColor);
					lineManager->DrawLine({ emitterPos.x, emitterPos.y - kMarkerSize, emitterPos.z }, { emitterPos.x, emitterPos.y + kMarkerSize, emitterPos.z }, shapeColor);
					lineManager->DrawLine({ emitterPos.x, emitterPos.y, emitterPos.z - kMarkerSize }, { emitterPos.x, emitterPos.y, emitterPos.z + kMarkerSize }, shapeColor);
					// ダイアモンド形状
					Vector3 top = { emitterPos.x, emitterPos.y + kMarkerSize, emitterPos.z };
					Vector3 bot = { emitterPos.x, emitterPos.y - kMarkerSize, emitterPos.z };
					lineManager->DrawLine({ emitterPos.x - kMarkerSize * 0.5f, emitterPos.y, emitterPos.z }, top, shapeColor);
					lineManager->DrawLine({ emitterPos.x + kMarkerSize * 0.5f, emitterPos.y, emitterPos.z }, top, shapeColor);
					lineManager->DrawLine({ emitterPos.x - kMarkerSize * 0.5f, emitterPos.y, emitterPos.z }, bot, shapeColor);
					lineManager->DrawLine({ emitterPos.x + kMarkerSize * 0.5f, emitterPos.y, emitterPos.z }, bot, shapeColor);
				}
				// Line - 始点から終点への線
				else if (type == SpawnShapeType::Line)
				{
					Vector3 lineStart = shape->GetLineStart();
					Vector3 lineEnd = shape->GetLineEnd();
					Vector3 start = emitterPos + lineStart;
					Vector3 end = emitterPos + lineEnd;
					
					// ライン本体
					lineManager->DrawLine(start, end, shapeColor);
					
					// 始点マーカー（緑）
					lineManager->DrawLine({ start.x - kSmallMarkerSize, start.y, start.z }, { start.x + kSmallMarkerSize, start.y, start.z }, shapeColor);
					lineManager->DrawLine({ start.x, start.y - kSmallMarkerSize, start.z }, { start.x, start.y + kSmallMarkerSize, start.z }, shapeColor);
					
					// 終点マーカー（黄色）
					lineManager->DrawLine({ end.x - kSmallMarkerSize, end.y, end.z }, { end.x + kSmallMarkerSize, end.y, end.z }, { 1.0f, 1.0f, 0.0f, 1.0f });
					lineManager->DrawLine({ end.x, end.y - kSmallMarkerSize, end.z }, { end.x, end.y + kSmallMarkerSize, end.z }, { 1.0f, 1.0f, 0.0f, 1.0f });
				}
				else if (type == SpawnShapeType::Sphere)
				{
					// 球体を描画（DrawSphereを使用）
					lineManager->DrawSphere(emitterPos, outerR, shapeColor);
					if (innerR > 0)
					{
						lineManager->DrawSphere(emitterPos, innerR, { shapeColor.x, shapeColor.y, shapeColor.z, 0.5f });
					}
				}
				else if (type == SpawnShapeType::Circle)
				{
					// 円を描画（XZ平面）
					for (int s = 0; s < kCircleSegments; ++s)
					{
						float a1 = (s / static_cast<float>(kCircleSegments)) * kTwoPi;
						float a2 = ((s + 1) / static_cast<float>(kCircleSegments)) * kTwoPi;
						
						// XZ平面の円
						lineManager->DrawLine(
							{ emitterPos.x + outerR * std::cos(a1), emitterPos.y, emitterPos.z + outerR * std::sin(a1) },
							{ emitterPos.x + outerR * std::cos(a2), emitterPos.y, emitterPos.z + outerR * std::sin(a2) },
							shapeColor
						);
						if (innerR > 0)
						{
							lineManager->DrawLine(
								{ emitterPos.x + innerR * std::cos(a1), emitterPos.y, emitterPos.z + innerR * std::sin(a1) },
								{ emitterPos.x + innerR * std::cos(a2), emitterPos.y, emitterPos.z + innerR * std::sin(a2) },
								shapeColor
							);
						}
					}
				}
				else if (type == SpawnShapeType::Box)
				{
					Vector3 boxSize = shape->GetBoxSize();
					Vector3 half = boxSize * 0.5f;
					
					// ボックスの8頂点
					Vector3 v0 = { emitterPos.x - half.x, emitterPos.y - half.y, emitterPos.z - half.z };
					Vector3 v1 = { emitterPos.x + half.x, emitterPos.y - half.y, emitterPos.z - half.z };
					Vector3 v2 = { emitterPos.x + half.x, emitterPos.y + half.y, emitterPos.z - half.z };
					Vector3 v3 = { emitterPos.x - half.x, emitterPos.y + half.y, emitterPos.z - half.z };
					Vector3 v4 = { emitterPos.x - half.x, emitterPos.y - half.y, emitterPos.z + half.z };
					Vector3 v5 = { emitterPos.x + half.x, emitterPos.y - half.y, emitterPos.z + half.z };
					Vector3 v6 = { emitterPos.x + half.x, emitterPos.y + half.y, emitterPos.z + half.z };
					Vector3 v7 = { emitterPos.x - half.x, emitterPos.y + half.y, emitterPos.z + half.z };
					
					// 底面
					lineManager->DrawLine(v0, v1, shapeColor);
					lineManager->DrawLine(v1, v2, shapeColor);
					lineManager->DrawLine(v2, v3, shapeColor);
					lineManager->DrawLine(v3, v0, shapeColor);
					// 上面
					lineManager->DrawLine(v4, v5, shapeColor);
					lineManager->DrawLine(v5, v6, shapeColor);
					lineManager->DrawLine(v6, v7, shapeColor);
					lineManager->DrawLine(v7, v4, shapeColor);
					// 縦線
					lineManager->DrawLine(v0, v4, shapeColor);
					lineManager->DrawLine(v1, v5, shapeColor);
					lineManager->DrawLine(v2, v6, shapeColor);
					lineManager->DrawLine(v3, v7, shapeColor);
				}
				else if (type == SpawnShapeType::Cone)
				{
					float height = shape->GetConeHeight();
					// コーンの底面円と頂点への線
					for (int s = 0; s < kConeSegments; ++s)
					{
						float a1 = (s / static_cast<float>(kConeSegments)) * kTwoPi;
						float a2 = ((s + 1) / static_cast<float>(kConeSegments)) * kTwoPi;
						
						Vector3 p1 = { emitterPos.x + outerR * std::cos(a1), emitterPos.y, emitterPos.z + outerR * std::sin(a1) };
						Vector3 p2 = { emitterPos.x + outerR * std::cos(a2), emitterPos.y, emitterPos.z + outerR * std::sin(a2) };
						Vector3 top = { emitterPos.x, emitterPos.y + height, emitterPos.z };
						
						lineManager->DrawLine(p1, p2, shapeColor);
						lineManager->DrawLine(p1, top, shapeColor);
					}
				}
			}
			// Attractor - ターゲット点への線
			else if (auto* attractor = dynamic_cast<const AttractorModule*>(module))
			{
				Vector3 target = attractor->GetTarget();
				Vector4 attractorColor = { 1.0f, 0.0f, 1.0f, 1.0f }; // マゼンタ
				lineManager->DrawLine(emitterPos, target, attractorColor);
				lineManager->DrawAxis(target, kMarkerSize);
			}
			// Vortex - 回転軸を表示
			else if (auto* vortex = dynamic_cast<const VortexModule*>(module))
			{
				Vector3 axis = vortex->GetAxis();
				Vector3 center = vortex->GetCenter();
				Vector4 vortexColor = { 1.0f, 0.5f, 0.0f, 1.0f }; // オレンジ
				
				Vector3 axisStart = center - axis * 2.0f;
				Vector3 axisEnd = center + axis * 2.0f;
				lineManager->DrawLine(axisStart, axisEnd, vortexColor);
			}
			// Orbit - 軌道軸を表示
			else if (auto* orbit = dynamic_cast<const OrbitModule*>(module))
			{
				Vector3 axis = orbit->GetOrbitAxis();
				Vector4 orbitColor = { 0.0f, 0.5f, 1.0f, 1.0f }; // 水色
				
				Vector3 axisStart = emitterPos - axis * 1.5f;
				Vector3 axisEnd = emitterPos + axis * 1.5f;
				lineManager->DrawLine(axisStart, axisEnd, orbitColor);
			}
		}

		// パーティクル位置を表示（クロスマーカー）
		const auto& particles = emitter->GetParticles();
		for (const auto& particle : particles)
		{
			if (!particle.IsAlive()) continue;

			// パーティクル位置に小さなクロスを描画
			Vector3 pos = particle.position;
			Vector4 color = { 0.0f, 1.0f, 1.0f, 1.0f };  // シアン

			lineManager->DrawLine(
				{ pos.x - kParticleMarkerSize, pos.y, pos.z },
				{ pos.x + kParticleMarkerSize, pos.y, pos.z },
				color
			);
			lineManager->DrawLine(
				{ pos.x, pos.y - kParticleMarkerSize, pos.z },
				{ pos.x, pos.y + kParticleMarkerSize, pos.z },
				color
			);
			lineManager->DrawLine(
				{ pos.x, pos.y, pos.z - kParticleMarkerSize },
				{ pos.x, pos.y, pos.z + kParticleMarkerSize },
				color
			);
		}
	}
#endif
}


void ParticleEditor::NewEffect()
{
	// 既存のエフェクトを削除
	if (currentEffect_)
	{
		ParticleManager::GetInstance()->RemoveEffect(currentEffect_);
		currentEffect_ = nullptr;
	}

	// 新規エフェクトを作成
	auto effect = std::make_unique<ParticleEffect>();
	effect->Initialize("NewEffect");
	effect->Play();
	
	// エディタ用なので自動削除しない
	effect->SetAutoRemove(false);

	// マネージャーに登録してポインタを保持
	currentEffect_ = effect.get();
	ParticleManager::GetInstance()->AddEffect(std::move(effect));

	// 選択状態とパスをリセット
	selectedEmitterIndex_ = -1;
	selectedModuleIndex_ = -1;
	effectPath_.clear();
	strcpy_s(effectNameBuffer_, "NewEffect");
}

void ParticleEditor::LoadEffect(const std::string& path)
{
	// ファイルからエフェクトを読み込み
	auto effect = ParticleEffect::LoadFromFile(path);
	if (effect)
	{
		// 既存のエフェクトを削除
		if (currentEffect_)
		{
			ParticleManager::GetInstance()->RemoveEffect(currentEffect_);
		}

		// エディタ用なので自動削除しない
		effect->SetAutoRemove(false);
		effectPath_ = path;
		strcpy_s(effectNameBuffer_, effect->GetName().c_str());
		effect->Play();

		// マネージャーに登録してポインタを保持
		currentEffect_ = effect.get();
		ParticleManager::GetInstance()->AddEffect(std::move(effect));
	}
}

void ParticleEditor::SaveEffect(const std::string& path)
{
	if (currentEffect_)
	{
		// 編集中の名前をエフェクトに反映
		if (effectNameBuffer_[0] != '\0')
		{
			currentEffect_->SetName(effectNameBuffer_);
		}

		// パスが空の場合はデフォルトパスを使用
		std::string savePath = path;
		if (savePath.empty())
		{
			// Resources/json/particle フォルダにエフェクト名で保存
			std::filesystem::path dir("Resources/json/particle");
			if (!std::filesystem::exists(dir))
			{
				std::filesystem::create_directories(dir);
			}
			savePath = (dir / (currentEffect_->GetName() + ".json")).string();
		}
		
		// ファイルに保存
		currentEffect_->SaveToFile(savePath);
		effectPath_ = savePath;
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
			
			// Load サブメニュー
			if (ImGui::BeginMenu("Load..."))
			{
				std::filesystem::path dir("Resources/Json/particle");
				if (std::filesystem::exists(dir))
				{
					for (const auto& entry : std::filesystem::directory_iterator(dir))
					{
						if (entry.path().extension() == ".json")
						{
							std::string filename = entry.path().filename().string();
							if (ImGui::MenuItem(filename.c_str()))
							{
								LoadEffect(entry.path().string());
							}
						}
					}
				}
				else
				{
					ImGui::TextDisabled("(No particle files found)");
				}
				ImGui::EndMenu();
			}
			
			ImGui::Separator();
			if (ImGui::MenuItem("Save")) { SaveEffect(effectPath_); }
			if (ImGui::MenuItem("Save As...")) 
			{ 
				// パスを空にして保存（デフォルトパスが使用される）
				effectPath_.clear();
				SaveEffect(""); 
			}
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
			ImGui::MenuItem("Show Debug Lines", nullptr, &showDebug_);
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
		// エフェクト名
		ImGui::Text("Name:");
		ImGui::SameLine(80);
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##Name", effectNameBuffer_, sizeof(effectNameBuffer_));

		if (currentEffect_)
		{
			// 位置
			Vector3 pos = currentEffect_->GetPosition();
			ImGui::Text("Position:");
			ImGui::SameLine(80);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::DragFloat3("##Position", &pos.x, 0.1f)) 
			{
				currentEffect_->SetPosition(pos);
			}

			// 再生状態
			ImGui::Text("Status:");
			ImGui::SameLine(80);
			bool isPlaying = currentEffect_->IsPlaying();
			if (ImGui::Checkbox("Playing", &isPlaying))
			{
				if (isPlaying) currentEffect_->Play();
				else currentEffect_->Stop();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reset")) 
			{ 
				currentEffect_->Reset(); 
				currentEffect_->Play(); 
			}

			// エミッター数
			ImGui::Text("Emitters:");
			ImGui::SameLine(80);
			ImGui::Text("%d", static_cast<int>(currentEffect_->GetEmitterCount()));

			ImGui::Separator();

			// 保存パス表示
			ImGui::Text("Path:");
			ImGui::SameLine(80);
			if (effectPath_.empty())
			{
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(not saved)");
			}
			else
			{
				ImGui::TextWrapped("%s", effectPath_.c_str());
			}

			// 保存ボタン
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
			if (ImGui::Button("Save Effect"))
			{
				SaveEffect(effectPath_);
			}
			ImGui::PopStyleColor();
			
			ImGui::SameLine();
			if (ImGui::Button("Save As..."))
			{
				effectPath_.clear();
				SaveEffect("");
			}
		}
	}
}

void ParticleEditor::DrawEmitterPanel()
{
	if (!currentEffect_) return;

	if (ImGui::CollapsingHeader("Emitters", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int emitterToDelete = -1;

		// エミッターリスト
		ImGui::BeginChild("EmitterList", ImVec2(0, 150), true);
		for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
		{
			auto* emitter = currentEffect_->GetEmitter(i);
			bool isSelected = (selectedEmitterIndex_ == static_cast<int>(i));

			ImGui::PushID(static_cast<int>(i));
			
			// 選択可能なアイテム
			if (ImGui::Selectable(emitter->GetName().c_str(), isSelected))
			{
				selectedEmitterIndex_ = static_cast<int>(i);
				selectedModuleIndex_ = -1;
			}

			// 右クリックコンテキストメニュー
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Delete"))
				{
					emitterToDelete = static_cast<int>(i);
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}
		ImGui::EndChild();

		// 削除処理（ループ外で行う）
		if (emitterToDelete >= 0)
		{
			currentEffect_->RemoveEmitter(static_cast<size_t>(emitterToDelete));
			if (selectedEmitterIndex_ == emitterToDelete)
			{
				selectedEmitterIndex_ = -1;
				selectedModuleIndex_ = -1;
			}
			else if (selectedEmitterIndex_ > emitterToDelete)
			{
				selectedEmitterIndex_--;
			}
		}

		// ボタン行
		if (ImGui::Button("+ Add Emitter")) { showAddEmitterDialog_ = true; }
		
		// 選択中のエミッターがあれば削除ボタンを表示
		if (selectedEmitterIndex_ >= 0)
		{
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("Delete Selected"))
			{
				currentEffect_->RemoveEmitter(static_cast<size_t>(selectedEmitterIndex_));
				selectedEmitterIndex_ = -1;
				selectedModuleIndex_ = -1;
			}
			ImGui::PopStyleColor();
		}
	}

	// Emitter Settings
	if (selectedEmitterIndex_ >= 0)
	{
		auto* emitter = currentEffect_->GetEmitter(static_cast<size_t>(selectedEmitterIndex_));
		if (emitter && ImGui::CollapsingHeader("Emitter Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Position
			Vector3 pos = emitter->GetPosition();
			if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
			{
				emitter->SetPosition(pos);
			}

			// Max Particles
			int maxP = static_cast<int>(emitter->GetMaxParticles());
			if (ImGui::InputInt("Max Particles", &maxP))
			{
				emitter->SetMaxParticles(static_cast<uint32_t>((std::max)(100, maxP)));
			}

			// Follow Offset
			Vector3 offset = emitter->GetFollowOffset();
			if (ImGui::DragFloat3("Follow Offset##emitterSettings", &offset.x, 0.1f))
			{
				emitter->SetFollowOffset(offset);
			}
			ImGui::SetItemTooltip("Offset applied when following a target Transform or Emitter");

			// Follow Emitter (同じエフェクト内の別エミッターを追従)
			ImGui::Separator();
			ImGui::Text("Follow Other Emitter:");
			int followIdx = emitter->GetFollowEmitterIndex();
			
			// エミッターリストを作成（現在選択中のエミッター以外）
			std::vector<const char*> emitterNames;
			std::vector<int> emitterIndices;
			emitterNames.push_back("None");
			emitterIndices.push_back(-1);
			
			for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
			{
				if (static_cast<int>(i) != selectedEmitterIndex_) // 自分自身は除外
				{
					auto* e = currentEffect_->GetEmitter(i);
					if (e)
					{
						emitterNames.push_back(e->GetName().c_str());
						emitterIndices.push_back(static_cast<int>(i));
					}
				}
			}
			
			// 現在の選択を見つける
			int currentSelection = 0;
			for (size_t i = 0; i < emitterIndices.size(); ++i)
			{
				if (emitterIndices[i] == followIdx)
				{
					currentSelection = static_cast<int>(i);
					break;
				}
			}
			
			if (ImGui::Combo("Follow Emitter##emitterSettings", &currentSelection, emitterNames.data(), 
			                 static_cast<int>(emitterNames.size())))
			{
				emitter->SetFollowEmitterIndex(emitterIndices[currentSelection]);
			}
			ImGui::SetItemTooltip("Select another emitter to follow within this effect");

			// Simulation Space
			const char* spaces[] = { "World", "Local" };
			int space = static_cast<int>(emitter->GetSimulationSpace());
			if (ImGui::Combo("Simulation Space##emitterSettings", &space, spaces, 2))
			{
				emitter->SetSimulationSpace(static_cast<SimulationSpace>(space));
			}
		}
	}
}

void ParticleEditor::DrawModulePanel()
{
	if (!currentEffect_ || selectedEmitterIndex_ < 0) return;

	auto* emitter = currentEffect_->GetEmitter(static_cast<size_t>(selectedEmitterIndex_));
	if (!emitter) return;

	// Active Particles表示（シンプルに）
	ImGui::Text("Active Particles: %d", static_cast<int>(emitter->GetParticles().size()));
	ImGui::Separator();

	// モジュールリスト（Spawn/Updateで分離）
	if (ImGui::CollapsingHeader("Modules", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int moduleToDelete = -1;

		// Spawnモジュール
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
		ImGui::SeparatorText("Spawn Modules");
		ImGui::PopStyleColor();
		
		bool hasSpawnModules = false;
		for (size_t i = 0; i < emitter->GetModuleCount(); ++i)
		{
			auto* module = emitter->GetModule(i);
			if (!module || module->GetPhase() != ModulePhase::Spawn) continue;
			hasSpawnModules = true;

			ImGui::PushID(static_cast<int>(i));

			bool isSelected = (selectedModuleIndex_ == static_cast<int>(i));
			
			// 選択可能なモジュール名
			char label[256];
			snprintf(label, sizeof(label), "  %s", module->GetName());
			
			if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowItemOverlap))
			{
				// 既に選択されている場合はトグルで閉じる
				if (isSelected)
				{
					selectedModuleIndex_ = -1;
				}
				else
				{
					selectedModuleIndex_ = static_cast<int>(i);
				}
			}

			// 右クリックコンテキストメニュー
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Move Up") && i > 0) { emitter->MoveModuleUp(i); }
				if (ImGui::MenuItem("Move Down") && i < emitter->GetModuleCount() - 1) { emitter->MoveModuleDown(i); }
				ImGui::Separator();
				if (ImGui::MenuItem("Delete")) { moduleToDelete = static_cast<int>(i); }
				ImGui::EndPopup();
			}

			// 削除ボタン（行の右端に配置）
			ImGui::SameLine(ImGui::GetWindowWidth() - 40);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
			if (ImGui::SmallButton("X"))
			{
				moduleToDelete = static_cast<int>(i);
			}
			ImGui::PopStyleColor();

			// 選択中のモジュールのプロパティをすぐ下に表示
			if (isSelected)
			{
				ImGui::Indent(16.0f);
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.2f, 0.15f, 1.0f));
				ImGui::BeginChild("SpawnModuleProps", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
				DrawModuleProperties(module);
				ImGui::EndChild();
				ImGui::PopStyleColor();
				ImGui::Unindent(16.0f);
			}

			ImGui::PopID();
		}
		if (!hasSpawnModules)
		{
			ImGui::TextDisabled("  (No spawn modules)");
		}

		ImGui::Spacing();

		// Updateモジュール
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
		ImGui::SeparatorText("Update Modules");
		ImGui::PopStyleColor();
		
		bool hasUpdateModules = false;
		for (size_t i = 0; i < emitter->GetModuleCount(); ++i)
		{
			auto* module = emitter->GetModule(i);
			if (!module || module->GetPhase() != ModulePhase::Update) continue;
			hasUpdateModules = true;

			ImGui::PushID(static_cast<int>(i) + 1000); // 別のIDを使用

			bool isSelected = (selectedModuleIndex_ == static_cast<int>(i));
			
			char label[256];
			snprintf(label, sizeof(label), "  %s", module->GetName());
			
			if (ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_AllowItemOverlap))
			{
				// 既に選択されている場合はトグルで閉じる
				if (isSelected)
				{
					selectedModuleIndex_ = -1;
				}
				else
				{
					selectedModuleIndex_ = static_cast<int>(i);
				}
			}

			// 右クリックコンテキストメニュー
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Move Up") && i > 0) { emitter->MoveModuleUp(i); }
				if (ImGui::MenuItem("Move Down") && i < emitter->GetModuleCount() - 1) { emitter->MoveModuleDown(i); }
				ImGui::Separator();
				if (ImGui::MenuItem("Delete")) { moduleToDelete = static_cast<int>(i); }
				ImGui::EndPopup();
			}

			// 削除ボタン
			ImGui::SameLine(ImGui::GetWindowWidth() - 40);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
			if (ImGui::SmallButton("X"))
			{
				moduleToDelete = static_cast<int>(i);
			}
			ImGui::PopStyleColor();

			// 選択中のモジュールのプロパティをすぐ下に表示
			if (isSelected)
			{
				ImGui::Indent(16.0f);
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
				ImGui::BeginChild("UpdateModuleProps", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
				DrawModuleProperties(module);
				ImGui::EndChild();
				ImGui::PopStyleColor();
				ImGui::Unindent(16.0f);
			}

			ImGui::PopID();
		}
		if (!hasUpdateModules)
		{
			ImGui::TextDisabled("  (No update modules)");
		}

		// 削除処理（ループ外で実行）
		if (moduleToDelete >= 0)
		{
			emitter->RemoveModule(static_cast<size_t>(moduleToDelete));
			if (selectedModuleIndex_ == moduleToDelete)
			{
				selectedModuleIndex_ = -1;
			}
			else if (selectedModuleIndex_ > moduleToDelete)
			{
				selectedModuleIndex_--;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::Button("+ Add Module")) { showAddModuleDialog_ = true; }
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
		bool useInitial = m->GetUseInitialColor();
		if (ImGui::Checkbox("Use Initial Color", &useInitial)) 
		{ 
			m->SetUseInitialColor(useInitial); 
		}
		ImGui::SetItemTooltip("When enabled, fades from the color set by InitialColor module instead of Start Color");
		
		if (!useInitial)
		{
			Vector4 start = m->GetStartColor();
			if (ImGui::ColorEdit4("Start Color", &start.x)) { m->SetStartColor(start); }
		}
		Vector4 end = m->GetEndColor();
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
	else if (auto* m = dynamic_cast<SpawnShapeModule*>(module))
	{
		const char* shapeTypes[] = { "Point", "Sphere", "Circle", "Box", "Cone", "Line" };
		int shape = static_cast<int>(m->GetShapeType());
		if (ImGui::Combo("Shape Type", &shape, shapeTypes, 6))
		{
			m->SetShapeType(static_cast<SpawnShapeType>(shape));
		}

		// SpawnLocation (Volume/Surface/Edge)
		SpawnShapeType currentShape = m->GetShapeType();
		if (currentShape == SpawnShapeType::Sphere || currentShape == SpawnShapeType::Circle || 
		    currentShape == SpawnShapeType::Box)
		{
			const char* spawnLocations[] = { "Volume", "Surface", "Edge" };
			int loc = static_cast<int>(m->GetSpawnLocation());
			if (ImGui::Combo("Spawn Location", &loc, spawnLocations, 
			    (currentShape == SpawnShapeType::Box) ? 3 : 2)) // Edge only for Box
			{
				m->SetSpawnLocation(static_cast<SpawnLocation>(loc));
			}
			ImGui::SetItemTooltip("Volume: Fill interior, Surface: Outer shell only, Edge: Box edges only");
		}

		float innerRadius = m->GetInnerRadius();
		float outerRadius = m->GetOuterRadius();
		if (ImGui::DragFloat("Inner Radius", &innerRadius, 0.1f, 0.0f, 100.0f)) { m->SetInnerRadius(innerRadius); }
		if (ImGui::DragFloat("Outer Radius", &outerRadius, 0.1f, 0.0f, 100.0f)) { m->SetOuterRadius(outerRadius); }

		Vector3 boxSize = m->GetBoxSize();
		if (ImGui::DragFloat3("Box Size", &boxSize.x, 0.1f)) { m->SetBoxSize(boxSize); }

		float coneHeight = m->GetConeHeight();
		if (ImGui::DragFloat("Cone Height", &coneHeight, 0.1f, 0.0f, 100.0f)) { m->SetConeHeight(coneHeight); }

		bool emitFromSurface = m->GetEmitFromSurface();
		if (ImGui::Checkbox("Emit From Surface", &emitFromSurface)) { m->SetEmitFromSurface(emitFromSurface); }

		float initialSpeed = m->GetInitialSpeed();
		if (ImGui::DragFloat("Initial Speed", &initialSpeed, 0.1f, 0.0f, 100.0f)) { m->SetInitialSpeed(initialSpeed); }

		// Line用パラメータ
		Vector3 lineStart = m->GetLineStart();
		Vector3 lineEnd = m->GetLineEnd();
		if (ImGui::DragFloat3("Line Start", &lineStart.x, 0.1f)) { m->SetLine(lineStart, lineEnd); }
		if (ImGui::DragFloat3("Line End", &lineEnd.x, 0.1f)) { m->SetLine(lineStart, lineEnd); }
	}
	else if (auto* m = dynamic_cast<InitialRotationModule*>(module))
	{
		float minAngle = m->GetMinAngle();
		float maxAngle = m->GetMaxAngle();
		if (ImGui::DragFloat("Min Angle", &minAngle, 1.0f, 0.0f, 360.0f)) { m->SetRotationRange(minAngle, maxAngle); }
		if (ImGui::DragFloat("Max Angle", &maxAngle, 1.0f, 0.0f, 360.0f)) { m->SetRotationRange(minAngle, maxAngle); }
	}
	else if (auto* m = dynamic_cast<RotationOverLifetimeModule*>(module))
	{
		float speed = m->GetRotationSpeed();
		if (ImGui::DragFloat("Rotation Speed (deg/s)", &speed, 1.0f, -1000.0f, 1000.0f)) { m->SetRotationSpeed(speed); }
	}
	else if (auto* m = dynamic_cast<OrbitModule*>(module))
	{
		float speed = m->GetOrbitSpeed();
		if (ImGui::DragFloat("Orbit Speed (deg/s)", &speed, 1.0f, -360.0f, 360.0f)) { m->SetOrbitSpeed(speed); }

		Vector3 axis = m->GetOrbitAxis();
		if (ImGui::DragFloat3("Orbit Axis", &axis.x, 0.01f)) { m->SetOrbitAxis(axis); }
	}
	else if (auto* m = dynamic_cast<NoiseModule*>(module))
	{
		float strength = m->GetStrength();
		float frequency = m->GetFrequency();
		if (ImGui::DragFloat("Strength", &strength, 0.1f, 0.0f, 100.0f)) { m->SetStrength(strength); }
		if (ImGui::DragFloat("Frequency", &frequency, 0.1f, 0.0f, 10.0f)) { m->SetFrequency(frequency); }
	}
	else if (auto* m = dynamic_cast<VelocityLimitModule*>(module))
	{
		float maxSpeed = m->GetMaxSpeed();
		if (ImGui::DragFloat("Max Speed", &maxSpeed, 0.1f, 0.0f, 100.0f)) { m->SetMaxSpeed(maxSpeed); }
	}
	// Phase 3: New modules
	else if (auto* m = dynamic_cast<AccelerationModule*>(module))
	{
		Vector3 acc = m->GetAcceleration();
		if (ImGui::DragFloat3("Acceleration", &acc.x, 0.1f)) { m->SetAcceleration(acc); }
	}
	else if (auto* m = dynamic_cast<CurlNoiseModule*>(module))
	{
		float strength = m->GetStrength();
		if (ImGui::DragFloat("Strength", &strength, 0.1f, 0.0f, 50.0f)) { m->SetStrength(strength); }
		
		float frequency = m->GetFrequency();
		if (ImGui::DragFloat("Frequency", &frequency, 0.1f, 0.01f, 10.0f)) { m->SetFrequency(frequency); }
		
		int octaves = m->GetOctaves();
		if (ImGui::SliderInt("Octaves", &octaves, 1, 8)) { m->SetOctaves(octaves); }
		
		float scroll = m->GetScrollSpeed();
		if (ImGui::DragFloat("Scroll Speed", &scroll, 0.1f, 0.0f, 10.0f)) { m->SetScrollSpeed(scroll); }
	}
	else if (auto* m = dynamic_cast<SizeBySpeedModule*>(module))
	{
		float minSpd = m->GetMinSpeed();
		float maxSpd = m->GetMaxSpeed();
		if (ImGui::DragFloat("Min Speed", &minSpd, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpd, maxSpd); }
		if (ImGui::DragFloat("Max Speed", &maxSpd, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpd, maxSpd); }
		
		Vector3 minScale = m->GetMinScale();
		Vector3 maxScale = m->GetMaxScale();
		if (ImGui::DragFloat3("Min Scale", &minScale.x, 0.01f)) { m->SetScaleRange(minScale, maxScale); }
		if (ImGui::DragFloat3("Max Scale", &maxScale.x, 0.01f)) { m->SetScaleRange(minScale, maxScale); }
	}
	else if (auto* m = dynamic_cast<ColorBySpeedModule*>(module))
	{
		float minSpd = m->GetMinSpeed();
		float maxSpd = m->GetMaxSpeed();
		if (ImGui::DragFloat("Min Speed", &minSpd, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpd, maxSpd); }
		if (ImGui::DragFloat("Max Speed", &maxSpd, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpd, maxSpd); }
		
		Vector4 minColor = m->GetMinColor();
		Vector4 maxColor = m->GetMaxColor();
		if (ImGui::ColorEdit4("Min Color", &minColor.x)) { m->SetColorRange(minColor, maxColor); }
		if (ImGui::ColorEdit4("Max Color", &maxColor.x)) { m->SetColorRange(minColor, maxColor); }
	}
	else if (auto* m = dynamic_cast<CollisionModule*>(module))
	{
		const char* modes[] = { "Plane", "World", "Box" };
		int mode = static_cast<int>(m->GetMode());
		if (ImGui::Combo("Mode", &mode, modes, 3)) { m->SetMode(static_cast<CollisionMode>(mode)); }
		
		float bounce = m->GetBounce();
		if (ImGui::DragFloat("Bounce", &bounce, 0.01f, 0.0f, 1.0f)) { m->SetBounce(bounce); }
		
		float friction = m->GetFriction();
		if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 1.0f)) { m->SetFriction(friction); }
		
		if (m->GetMode() == CollisionMode::Plane || m->GetMode() == CollisionMode::World)
		{
			float height = m->GetPlaneHeight();
			if (ImGui::DragFloat("Plane Height", &height, 0.1f)) { m->SetPlaneHeight(height); }
		}
		else if (m->GetMode() == CollisionMode::Box)
		{
			Vector3 center = m->GetBoxCenter();
			Vector3 size = m->GetBoxSize();
			if (ImGui::DragFloat3("Box Center", &center.x, 0.1f)) { m->SetBoxCenter(center); }
			if (ImGui::DragFloat3("Box Size", &size.x, 0.1f)) { m->SetBoxSize(size); }
		}
		
		bool killOnCol = m->GetKillOnCollision();
		if (ImGui::Checkbox("Kill On Collision", &killOnCol)) { m->SetKillOnCollision(killOnCol); }
	}
	else if (auto* m = dynamic_cast<KillZoneModule*>(module))
	{
		const char* zoneTypes[] = { "Box", "Sphere" };
		int zType = static_cast<int>(m->GetZoneType());
		if (ImGui::Combo("Zone Type", &zType, zoneTypes, 2)) { m->SetZoneType(static_cast<KillZoneType>(zType)); }
		
		Vector3 center = m->GetCenter();
		if (ImGui::DragFloat3("Center", &center.x, 0.1f)) { m->SetCenter(center); }
		
		if (m->GetZoneType() == KillZoneType::Box)
		{
			Vector3 size = m->GetBoxSize();
			if (ImGui::DragFloat3("Size", &size.x, 0.1f)) { m->SetBoxSize(size); }
		}
		else
		{
			float radius = m->GetRadius();
			if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.0f, 100.0f)) { m->SetRadius(radius); }
		}
		
		bool killInside = m->GetKillInside();
		if (ImGui::Checkbox("Kill Inside", &killInside)) { m->SetKillInside(killInside); }
		ImGui::SetItemTooltip("ON: Kill particles inside zone, OFF: Kill particles outside zone");
	}
	else if (auto* m = dynamic_cast<SprintToTargetModule*>(module))
	{
		Vector3 target = m->GetTarget();
		if (ImGui::DragFloat3("Target", &target.x, 0.1f)) { m->SetTarget(target); }
		
		float acc = m->GetAcceleration();
		if (ImGui::DragFloat("Acceleration", &acc, 0.1f, 0.0f, 50.0f)) { m->SetAcceleration(acc); }
		
		float arriveRad = m->GetArriveRadius();
		if (ImGui::DragFloat("Arrive Radius", &arriveRad, 0.1f, 0.0f, 10.0f)) { m->SetArriveRadius(arriveRad); }
		
		bool killOnArrive = m->GetKillOnArrive();
		if (ImGui::Checkbox("Kill On Arrive", &killOnArrive)) { m->SetKillOnArrive(killOnArrive); }
		
		bool useSpeedCurve = m->GetUseSpeedCurve();
		if (ImGui::Checkbox("Use Speed Curve", &useSpeedCurve)) { m->SetUseSpeedCurve(useSpeedCurve); }
		
		if (useSpeedCurve)
		{
			float maxDist = m->GetMaxDistance();
			if (ImGui::DragFloat("Max Distance", &maxDist, 0.1f, 1.0f, 100.0f)) { m->SetMaxDistance(maxDist); }
			
			float speedBoost = m->GetSpeedBoost();
			if (ImGui::DragFloat("Speed Boost", &speedBoost, 0.1f, 0.0f, 10.0f)) { m->SetSpeedBoost(speedBoost); }
		}
	}
	// Phase 4: Sub-Emitters
	else if (auto* m = dynamic_cast<SubEmitterModule*>(module))
	{
		ImGui::Text("Sub Emitter Configurations: %d", static_cast<int>(m->GetConfigCount()));
		
		// 既存設定の編集
		for (size_t i = 0; i < m->GetConfigCount(); ++i)
		{
			auto* config = m->GetConfig(i);
			if (!config) continue;
			
			ImGui::PushID(static_cast<int>(i));
			ImGui::Separator();
			ImGui::Text("Config %d", static_cast<int>(i + 1));
			
			// Effect Path
			static char pathBuffer[256];
			strncpy_s(pathBuffer, config->effectPath.c_str(), sizeof(pathBuffer) - 1);
			if (ImGui::InputText("Effect Path", pathBuffer, sizeof(pathBuffer)))
			{
				config->effectPath = pathBuffer;
			}
			ImGui::SetItemTooltip("Path to sub-effect JSON file");
			
			// Trigger
			const char* triggers[] = { "OnSpawn", "OnDeath", "OnCollision", "Continuous" };
			int trigger = static_cast<int>(config->trigger);
			if (ImGui::Combo("Trigger", &trigger, triggers, 4))
			{
				config->trigger = static_cast<SubEmitterTrigger>(trigger);
			}
			
			// Probability
			if (ImGui::DragFloat("Probability", &config->probability, 0.01f, 0.0f, 1.0f))
			{
				config->probability = (std::clamp)(config->probability, 0.0f, 1.0f);
			}
			
			// Continuous rate (only for Continuous trigger)
			if (config->trigger == SubEmitterTrigger::Continuous)
			{
				ImGui::DragFloat("Rate (per sec)", &config->continuousRate, 0.1f, 0.1f, 100.0f);
			}
			
			// Inheritance settings
			ImGui::Checkbox("Inherit Position", &config->inheritPosition);
			ImGui::Checkbox("Inherit Velocity", &config->inheritVelocity);
			if (config->inheritVelocity)
			{
				ImGui::DragFloat("Velocity Scale", &config->inheritVelocityScale, 0.1f, 0.0f, 2.0f);
			}
			ImGui::Checkbox("Inherit Color", &config->inheritColor);
			ImGui::Checkbox("Inherit Scale", &config->inheritScale);
			
			// Delete button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("Delete Config"))
			{
				m->RemoveConfig(i);
				ImGui::PopStyleColor();
				ImGui::PopID();
				break; // List changed, exit loop
			}
			ImGui::PopStyleColor();
			
			ImGui::PopID();
		}
		
		ImGui::Separator();
		if (ImGui::Button("+ Add Config"))
		{
			SubEmitterConfig newConfig;
			newConfig.effectPath = "./Resources/json/particle/sub_effect.json";
			m->AddConfig(newConfig);
		}
	}
	// Trail Module
	else if (auto* m = dynamic_cast<TrailModule*>(module))
	{
		// トレイル生成レート
		float trailRate = m->GetTrailRate();
		if (ImGui::DragFloat("Trail Rate", &trailRate, 1.0f, 1.0f, 120.0f))
		{
			m->SetTrailRate(trailRate);
		}
		ImGui::SetItemTooltip("Trail particles per second");

		// トレイル寿命
		float trailLifetime = m->GetTrailLifetime();
		if (ImGui::DragFloat("Trail Lifetime", &trailLifetime, 0.1f, 0.1f, 10.0f))
		{
			m->SetTrailLifetime(trailLifetime);
		}

		// トレイル幅
		float trailWidth = m->GetTrailWidth();
		if (ImGui::DragFloat("Trail Width", &trailWidth, 0.01f, 0.01f, 5.0f))
		{
			m->SetTrailWidth(trailWidth);
		}

		// 最小距離
		float minDistance = m->GetMinDistance();
		if (ImGui::DragFloat("Min Distance", &minDistance, 0.01f, 0.01f, 1.0f))
		{
			m->SetMinDistance(minDistance);
		}
		ImGui::SetItemTooltip("Minimum distance before spawning a new trail point");

		// 色継承
		bool inheritColor = m->GetInheritColor();
		if (ImGui::Checkbox("Inherit Color", &inheritColor))
		{
			m->SetInheritColor(inheritColor);
		}

		// トレイル色（inheritColor=false時のみ）
		if (!inheritColor)
		{
			Vector4 trailColor = m->GetTrailColor();
			if (ImGui::ColorEdit4("Trail Color", &trailColor.x))
			{
				m->SetTrailColor(trailColor);
			}
		}
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

	ImGui::Spacing();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
	ImGui::SeparatorText("Renderer");
	ImGui::PopStyleColor();
	
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
						newRenderer->Initialize("./Resources/uvChecker.png");
						newRenderer->SetBlendMode(renderer->GetBlendMode());
						emitter->SetRenderer(std::move(newRenderer));
						renderer = emitter->GetRenderer();
					}
					else if (newType == RendererType::Ribbon)
					{
						auto newRenderer = std::make_unique<TrailRenderer>();
						newRenderer->Initialize("./Resources/uvChecker.png");
						newRenderer->SetBlendMode(renderer->GetBlendMode());
						emitter->SetRenderer(std::move(newRenderer));
						renderer = emitter->GetRenderer();
					}
					else if (newType == RendererType::Mesh)
					{
						auto newRenderer = std::make_unique<MeshRenderer>();
						newRenderer->Initialize("./Resources/uvChecker.png");
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
			// Trail Renderer特有の設定
			else if (auto* trailRenderer = dynamic_cast<TrailRenderer*>(renderer))
			{
				ImGui::Separator();
				ImGui::Text("Trail Settings:");

				// トレイル幅
				float width = trailRenderer->GetTrailWidth();
				if (ImGui::DragFloat("Trail Width", &width, 0.01f, 0.01f, 10.0f))
				{
					trailRenderer->SetTrailWidth(width);
				}

				// トレイル寿命
				float trailLifetime = trailRenderer->GetTrailLifetime();
				if (ImGui::DragFloat("Trail Lifetime", &trailLifetime, 0.1f, 0.1f, 10.0f))
				{
					trailRenderer->SetTrailLifetime(trailLifetime);
				}
				ImGui::SetItemTooltip("How long the trail persists (seconds)");

				// 記録間隔
				float recordInterval = trailRenderer->GetRecordInterval();
				if (ImGui::DragFloat("Record Interval", &recordInterval, 0.001f, 0.001f, 0.1f))
				{
					trailRenderer->SetRecordInterval(recordInterval);
				}
				ImGui::SetItemTooltip("Time between position samples (lower = smoother)");

				// 最小セグメント距離
				float minDist = trailRenderer->GetMinSegmentDistance();
				if (ImGui::DragFloat("Min Segment Distance", &minDist, 0.01f, 0.01f, 1.0f))
				{
					trailRenderer->SetMinSegmentDistance(minDist);
				}

				// テクスチャモード
				const char* textureModes[] = { "Stretch", "Tile" };
				int texMode = static_cast<int>(trailRenderer->GetTextureMode());
				if (ImGui::Combo("Texture Mode", &texMode, textureModes, 2))
				{
					trailRenderer->SetTextureMode(static_cast<RibbonTextureMode>(texMode));
				}

				// タイルスケール（Tileモードのみ）
				if (trailRenderer->GetTextureMode() == RibbonTextureMode::Tile)
				{
					float tileScale = trailRenderer->GetTileScale();
					if (ImGui::DragFloat("Tile Scale", &tileScale, 0.1f, 0.1f, 100.0f))
					{
						trailRenderer->SetTileScale(tileScale);
					}
				}

				ImGui::Separator();
				ImGui::Text("Fade Settings:");

				// 幅フェード
				bool widthFade = trailRenderer->GetWidthFade();
				if (ImGui::Checkbox("Width Fade", &widthFade))
				{
					trailRenderer->SetWidthFade(widthFade);
				}
				ImGui::SetItemTooltip("Trail gets thinner towards the end");

				// アルファフェード
				bool alphaFade = trailRenderer->GetAlphaFade();
				if (ImGui::Checkbox("Alpha Fade", &alphaFade))
				{
					trailRenderer->SetAlphaFade(alphaFade);
				}
				ImGui::SetItemTooltip("Trail becomes transparent towards the end");

				// ビルボード設定
				bool billboard = trailRenderer->GetBillboard();
				if (ImGui::Checkbox("Billboard", &billboard))
				{
					trailRenderer->SetBillboard(billboard);
				}
				ImGui::SetItemTooltip("Enable billboard facing for trail segments");

				// TextureManagerから読み込み済みテクスチャを取得
				auto texturePaths = TextureManager::GetInstance()->GetLoadedTexturePaths();

				if (!texturePaths.empty())
				{
					ImGui::Separator();
					ImGui::Text("Texture:");

					static int selectedTrailTextureIdx = 0;

					if (ImGui::BeginCombo("##TrailTexture", texturePaths.empty() ? "(None)" : texturePaths[selectedTrailTextureIdx].c_str()))
					{
						for (int i = 0; i < static_cast<int>(texturePaths.size()); ++i)
						{
							bool isSelected = (selectedTrailTextureIdx == i);

							std::string displayName = texturePaths[i];
							size_t lastSlash = displayName.find_last_of("/\\");
							if (lastSlash != std::string::npos)
							{
								displayName = displayName.substr(lastSlash + 1);
							}
							if (displayName.empty()) displayName = "Unknown";

							std::string label = displayName + "##trail" + std::to_string(i);

							if (ImGui::Selectable(label.c_str(), isSelected))
							{
								selectedTrailTextureIdx = i;
								trailRenderer->SetTexture(texturePaths[i]);
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
			else if (auto* meshRenderer = dynamic_cast<MeshRenderer*>(renderer))
			{
				ImGui::Separator();
				ImGui::Text("Mesh Settings:");

				const char* primitiveTypes[] = { "Plane", "Ring", "Cylinder", "Sphere", "Torus", "Star", "Heart", "Spiral", "Cone", "Cube" };
				int primType = static_cast<int>(meshRenderer->GetPrimitiveType());
				PrimitiveOptions options = meshRenderer->GetOptions();
				bool optionsChanged = false;

				if (ImGui::Combo("Primitive", &primType, primitiveTypes, 10))
				{
					meshRenderer->SetPrimitive(static_cast<PrimitiveType>(primType), options);
				}

				// プリミティブタイプに応じたオプションを表示
				PrimitiveType currentType = static_cast<PrimitiveType>(primType);
				
				// Segments (共通)
				if (currentType != PrimitiveType::Plane && currentType != PrimitiveType::Cube)
				{
					int segments = static_cast<int>(options.segments);
					if (ImGui::SliderInt("Segments", &segments, 4, 64))
					{
						options.segments = static_cast<uint32_t>(segments);
						optionsChanged = true;
					}
				}

				// Rings (Sphere)
				if (currentType == PrimitiveType::Sphere)
				{
					int rings = static_cast<int>(options.rings);
					if (ImGui::SliderInt("Rings", &rings, 4, 32))
					{
						options.rings = static_cast<uint32_t>(rings);
						optionsChanged = true;
					}
				}

				// Inner/Outer Radius (Ring, Star, Torus)
				if (currentType == PrimitiveType::Ring || currentType == PrimitiveType::Star)
				{
					if (ImGui::DragFloat("Inner Radius", &options.innerRadius, 0.01f, 0.0f, 1.0f))
					{
						optionsChanged = true;
					}
					if (ImGui::DragFloat("Outer Radius", &options.outerRadius, 0.01f, 0.1f, 2.0f))
					{
						optionsChanged = true;
					}
				}

				// Tube Radius (Torus)
				if (currentType == PrimitiveType::Torus)
				{
					if (ImGui::DragFloat("Tube Radius", &options.tubeRadius, 0.01f, 0.05f, 0.5f))
					{
						optionsChanged = true;
					}
				}

				// Points (Star)
				if (currentType == PrimitiveType::Star)
				{
					int points = static_cast<int>(options.points);
					if (ImGui::SliderInt("Points", &points, 3, 12))
					{
						options.points = static_cast<uint32_t>(points);
						optionsChanged = true;
					}
				}

				// Turns (Spiral)
				if (currentType == PrimitiveType::Spiral)
				{
					if (ImGui::DragFloat("Turns", &options.turns, 0.1f, 0.5f, 10.0f))
					{
						optionsChanged = true;
					}
				}

				// Caps (Cylinder, Cone)
				if (currentType == PrimitiveType::Cylinder || currentType == PrimitiveType::Cone)
				{
					if (ImGui::Checkbox("With Caps", &options.withCaps))
					{
						optionsChanged = true;
					}
				}

				// Double Sided (Plane)
				if (currentType == PrimitiveType::Plane)
				{
					if (ImGui::Checkbox("Double Sided", &options.doubleSided))
					{
						optionsChanged = true;
					}
				}

				// オプション変更適用
				if (optionsChanged)
				{
					meshRenderer->SetPrimitive(currentType, options);
				}

				float scale = meshRenderer->GetScale();
				if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.01f, 10.0f))
				{
					meshRenderer->SetScale(scale);
				}

				// ビルボード設定
				bool useBillboard = meshRenderer->GetBillboard();
				if (ImGui::Checkbox("Billboard", &useBillboard))
				{
					meshRenderer->SetBillboard(useBillboard);
				}

				// ティントカラー
				Vector4 tintColor = meshRenderer->GetTintColor();
				if (ImGui::ColorEdit4("Tint Color", &tintColor.x))
				{
					meshRenderer->SetTintColor(tintColor);
				}

				// TextureManagerから読み込み済みテクスチャを取得
				auto texturePaths = TextureManager::GetInstance()->GetLoadedTexturePaths();

				if (!texturePaths.empty())
				{
					ImGui::Text("Texture:");

					static int selectedMeshTextureIdx = 0;

					if (ImGui::BeginCombo("##MeshTexture", texturePaths.empty() ? "(None)" : texturePaths[selectedMeshTextureIdx].c_str()))
					{
						for (int i = 0; i < static_cast<int>(texturePaths.size()); ++i)
						{
							bool isSelected = (selectedMeshTextureIdx == i);

							std::string displayName = texturePaths[i];
							size_t lastSlash = displayName.find_last_of("/\\");
							if (lastSlash != std::string::npos)
							{
								displayName = displayName.substr(lastSlash + 1);
							}
							if (displayName.empty()) displayName = "Unknown";

							std::string label = displayName + "##mesh" + std::to_string(i);

							if (ImGui::Selectable(label.c_str(), isSelected))
							{
								selectedMeshTextureIdx = i;
								meshRenderer->SetTexture(texturePaths[i]);
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
		}
		else
		{
			ImGui::Text("No renderer assigned");

			if (ImGui::Button("Create Sprite Renderer"))
			{
				auto newRenderer = std::make_unique<SpriteRenderer>();
				newRenderer->Initialize("./Resources/uvChecker.png");
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
			renderer->Initialize("./Resources/uvChecker.png");
			emitter->SetRenderer(std::move(renderer));

			// モジュールは手動で追加する（初期モジュールなし）

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
		static int selectedCategory = 0; // 0=Spawn, 1=Update
		ImGui::RadioButton("Spawn", &selectedCategory, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Update", &selectedCategory, 1);
		ImGui::Separator();

		static int selectedModule = 0;

		if (selectedCategory == 0)
		{
			// Spawn モジュール
			const char* spawnModules[] = {
				"Spawn Rate",
				"Spawn Burst",
				"Spawn Shape",
				"Initial Lifetime",
				"Initial Velocity",
				"Initial Scale",
				"Initial Color",
				"Initial Rotation"
			};
			const char* spawnDescriptions[] = {
				"Spawn particles at a constant rate",
				"Spawn multiple particles at once at intervals",
				"Spawn particles from shapes (Box, Sphere, Cone, etc.)",
				"Set initial lifetime of particles",
				"Set initial velocity and direction",
				"Set initial size of particles",
				"Set initial color (RGBA)",
				"Set initial rotation angle"
			};
			
			ImGui::Combo("Spawn Module", &selectedModule, spawnModules, IM_ARRAYSIZE(spawnModules));
			
			// 説明表示
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", 
				(selectedModule >= 0 && selectedModule < IM_ARRAYSIZE(spawnDescriptions)) 
				? spawnDescriptions[selectedModule] : "");

			if (ImGui::Button("Add"))
			{
				switch (selectedModule)
				{
				case 0: emitter->AddModule(std::make_unique<SpawnRateModule>()); break;
				case 1: emitter->AddModule(std::make_unique<SpawnBurstModule>()); break;
				case 2: emitter->AddModule(std::make_unique<SpawnShapeModule>()); break;
				case 3: emitter->AddModule(std::make_unique<InitialLifetimeModule>()); break;
				case 4: emitter->AddModule(std::make_unique<InitialVelocityModule>()); break;
				case 5: emitter->AddModule(std::make_unique<InitialScaleModule>()); break;
				case 6: emitter->AddModule(std::make_unique<InitialColorModule>()); break;
				case 7: emitter->AddModule(std::make_unique<InitialRotationModule>()); break;
				}
				showAddModuleDialog_ = false;
			}
		}
		else
		{
			// Update モジュール
			const char* updateModules[] = {
				"Gravity",
				"Drag",
				"Color Fade",
				"Scale Over Lifetime",
				"Rotation Over Lifetime",
				"Texture Sheet",
				"Attractor",
				"Vortex",
				"Orbit",
				"Noise",
				"Velocity Limit",
				"Acceleration",
				"Curl Noise",
				"Size By Speed",
				"Color By Speed",
				"Collision",
				"Kill Zone",
				"Sprint To Target",
				"Sub Emitter",
				"Trail"
			};
			const char* updateDescriptions[] = {
				"Apply gravity (downward Y-axis)",
				"Apply air resistance to slow particles",
				"Fade color over lifetime",
				"Change scale over lifetime",
				"Rotate over lifetime",
				"UV animation for sprite sheets",
				"Attract/repel to a point",
				"Swirl particles in a vortex",
				"Orbit around a center point",
				"Add random noise movement",
				"Limit maximum speed",
				"Apply constant acceleration",
				"Add turbulence with 3D curl noise",
				"Change size based on speed",
				"Change color based on speed",
				"Collide and bounce off planes/boxes",
				"Kill particles inside/outside a zone",
				"Accelerate toward a target position",
				"Spawn sub-effects on particle events",
				"Generate trail particles following parent"
			};
			
			ImGui::Combo("Update Module", &selectedModule, updateModules, IM_ARRAYSIZE(updateModules));
			
			// 説明表示
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s",
				(selectedModule >= 0 && selectedModule < IM_ARRAYSIZE(updateDescriptions))
				? updateDescriptions[selectedModule] : "");

			if (ImGui::Button("Add"))
			{
				switch (selectedModule)
				{
				case 0: emitter->AddModule(std::make_unique<GravityModule>()); break;
				case 1: emitter->AddModule(std::make_unique<DragModule>()); break;
				case 2: emitter->AddModule(std::make_unique<ColorFadeModule>()); break;
				case 3: emitter->AddModule(std::make_unique<ScaleOverLifetimeModule>()); break;
				case 4: emitter->AddModule(std::make_unique<RotationOverLifetimeModule>()); break;
				case 5: emitter->AddModule(std::make_unique<TextureSheetModule>()); break;
				case 6: emitter->AddModule(std::make_unique<AttractorModule>()); break;
				case 7: emitter->AddModule(std::make_unique<VortexModule>()); break;
				case 8: emitter->AddModule(std::make_unique<OrbitModule>()); break;
				case 9: emitter->AddModule(std::make_unique<NoiseModule>()); break;
				case 10: emitter->AddModule(std::make_unique<VelocityLimitModule>()); break;
				case 11: emitter->AddModule(std::make_unique<AccelerationModule>()); break;
				case 12: emitter->AddModule(std::make_unique<CurlNoiseModule>()); break;
				case 13: emitter->AddModule(std::make_unique<SizeBySpeedModule>()); break;
				case 14: emitter->AddModule(std::make_unique<ColorBySpeedModule>()); break;
				case 15: emitter->AddModule(std::make_unique<CollisionModule>()); break;
				case 16: emitter->AddModule(std::make_unique<KillZoneModule>()); break;
				case 17: emitter->AddModule(std::make_unique<SprintToTargetModule>()); break;
				case 18: emitter->AddModule(std::make_unique<SubEmitterModule>()); break;
				case 19: emitter->AddModule(std::make_unique<TrailModule>()); break;
				}
				showAddModuleDialog_ = false;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { showAddModuleDialog_ = false; }

		ImGui::EndPopup();
	}
}
#endif
