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
#include "effects/particle/module/spawn/SubEmitterModule.h"
#include "effects/particle/module/update/MotionEffectModules.h"
#include "effects/particle/module/update/NaturalBehaviorModules.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"
#include "manager/scene/CameraManager.h"
#include "manager/graphics/TextureManager.h"
#include "manager/graphics/LineManager.h"
#include "manager/effect/ParticlePipelineManager.h"
#include "math/BlendMode.h"
#include "time/TimeManager.h"
#include "time/Timer.h"
#include <filesystem>
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"

#endif

namespace KCE
{

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
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance()) {
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
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

#ifdef USE_IMGUI
	DebugUIManager::GetInstance()->RegisterWindow(this, "パーティクルエディター###Particle Editor", [this]() { this->DrawImGui(); }, EditorDock::Bottom, false);
#endif
}

void ParticleEditor::DrawImGui()
{
#ifdef USE_IMGUI
	// メニューバー描画
	DrawMenuBar();

	// エフェクト全体のパネル
	DrawEffectPanel();
	DrawPreviewPanel();
	DrawEmitterPanel();
	
	// エミッター選択時のみモジュールとレンダラーを表示
	if (selectedEmitterIndex_ >= 0)
	{
		DrawModulePanel();
		DrawRendererPanel();
	}

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
#endif
}

void ParticleEditor::Update(CameraManager* camera)
{
	(void)camera;
	float dt = TimeManager::GetInstance().GetGameContext().deltaTime;

	// ループプレビュー：一定間隔で全エミッターを再スタート
	if (previewLooping_ && currentEffect_)
	{
		previewElapsed_ += dt;
		if (previewElapsed_ >= previewRepeatInterval_)
		{
			previewElapsed_ = 0.0f;
			currentEffect_->Play();
		}
	}

	// トレイル/マルチソースプレビュー用のダミーモーション更新
	if (enablePreviewTarget_ && currentEffect_)
	{
		previewTargetTime_ += dt;

		// 3次元のループ軌跡（円運動 ＋ サイン波の高さ変化）を計算
		float angle = previewTargetTime_ * previewTargetSpeed_;
		previewTargetPos_.x = std::cos(angle) * previewTargetRadius_;
		previewTargetPos_.z = std::sin(angle) * previewTargetRadius_;
		previewTargetPos_.y = std::sin(angle * 2.0f) * (previewTargetRadius_ * 0.5f) + previewTargetRadius_;

		// エフェクト自体の位置を動かす（エミッター位置を動かすことで通常トレイルをプレビュー可能にする）
		currentEffect_->SetPosition(previewTargetPos_);

		// ソースが未登録なら新規登録
		if (previewSourceId_ == 0)
		{
			previewSourceId_ = currentEffect_->RegisterSourceManual();
		}

		// 登録済みのソース座標を更新
		if (previewSourceId_ != 0)
		{
			currentEffect_->UpdateSourcePosition(previewSourceId_, previewTargetPos_);
		}
	}
	else
	{
		// OFF またはエフェクト消失時にソースを安全に解除
		if (previewSourceId_ != 0 && currentEffect_)
		{
			currentEffect_->UnregisterSource(previewSourceId_);
		}
		previewSourceId_ = 0;
		previewTargetTime_ = 0.0f;
	}
}

void ParticleEditor::DrawDebug()
{
#ifdef USE_IMGUI
	if (!isVisible_ || !showDebug_ || !currentEffect_) return;

	auto* lineManager = LineManager::GetInstance();
	if (!lineManager) return;

	// トレイルプレビュー用のダミーターゲット位置を可視化（マゼンタ色の軸とマーカー）
	if (enablePreviewTarget_)
	{
		lineManager->DrawAxis(previewTargetPos_, 0.8f);
		Vector4 debugColor = { 1.0f, 0.0f, 1.0f, 0.5f }; // 半透明マゼンタ
		lineManager->DrawLine({ previewTargetPos_.x - 0.2f, previewTargetPos_.y, previewTargetPos_.z }, { previewTargetPos_.x + 0.2f, previewTargetPos_.y, previewTargetPos_.z }, debugColor);
		lineManager->DrawLine({ previewTargetPos_.x, previewTargetPos_.y - 0.2f, previewTargetPos_.z }, { previewTargetPos_.x, previewTargetPos_.y + 0.2f, previewTargetPos_.z }, debugColor);
		lineManager->DrawLine({ previewTargetPos_.x, previewTargetPos_.y, previewTargetPos_.z - 0.2f }, { previewTargetPos_.x, previewTargetPos_.y, previewTargetPos_.z + 0.2f }, debugColor);
	}

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
		if (showParticleMarkers_)
		{
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
	if (ImGui::CollapsingHeader("メニュー / 操作", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button("新しいエフェクト")) { NewEffect(); }
		ImGui::SameLine();
		if (ImGui::Button("保存")) { SaveEffect(effectPath_); }
		ImGui::SameLine();
		if (ImGui::Button("名前を付けて保存...")) { effectPath_.clear(); SaveEffect(""); }
		ImGui::SameLine();

		if (ImGui::Button("読み込み..."))
		{
			ImGui::OpenPopup("LoadEffectPopup");
		}
		if (ImGui::BeginPopup("LoadEffectPopup"))
		{
			std::filesystem::path dir("Resources/Json/particle");
			if (std::filesystem::exists(dir))
			{
				for (const auto& entry : std::filesystem::directory_iterator(dir))
				{
					if (entry.path().extension() == ".json")
					{
						std::string filename = entry.path().filename().string();
						if (ImGui::Selectable(filename.c_str()))
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
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("エミッターを追加")) { showAddEmitterDialog_ = true; }
		ImGui::SameLine();
		if (ImGui::Button("表示をリセット")) { if (currentEffect_) { currentEffect_->Reset(); currentEffect_->Play(); } }
		
		ImGui::Checkbox("デバッグ線を表示", &showDebug_);
		ImGui::SameLine();
		ImGui::Checkbox("パーティクルの目印を表示", &showParticleMarkers_);

		// Skydome Color Tint
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		ImGui::ColorEdit3("スカイドームの色", &skydomeColor_.x);
	}
}

void ParticleEditor::DrawEffectPanel()
{
	if (ImGui::CollapsingHeader("エフェクト", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// エフェクト名
		ImGui::Text("名前:");
		ImGui::SameLine(80);
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##Name", effectNameBuffer_, sizeof(effectNameBuffer_));

		if (currentEffect_)
		{
			// 位置
			Vector3 pos = currentEffect_->GetPosition();
			ImGui::Text("位置:");
			ImGui::SameLine(80);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::DragFloat3("##Position", &pos.x, 0.1f)) 
			{
				currentEffect_->SetPosition(pos);
			}

			// 再生状態
			ImGui::Text("状態:");
			ImGui::SameLine(80);
			bool isPlaying = currentEffect_->IsPlaying();
			if (ImGui::Checkbox("再生中", &isPlaying))
			{
				if (isPlaying) currentEffect_->Play();
				else currentEffect_->Stop();
			}
			ImGui::SameLine();
			if (ImGui::Button("リセット")) 
			{ 
				currentEffect_->Reset(); 
				currentEffect_->Play(); 
			}

			ImGui::Text("時間:");
			ImGui::SameLine(80);
			int deltaType = static_cast<int>(currentEffect_->GetDeltaTimeType());
			const char* deltaTypeNames[] = { "DeltaTime", "RealDeltaTime" };
			if (ImGui::Combo("##DeltaTimeType", &deltaType, deltaTypeNames, 2))
			{
				currentEffect_->SetDeltaTimeType(static_cast<DeltaTimeType>(deltaType));
			}

			// エミッター数
			ImGui::Text("エミッター数:");
			ImGui::SameLine(80);
			ImGui::Text("%d", static_cast<int>(currentEffect_->GetEmitterCount()));

			ImGui::Separator();

			// 保存パス表示
			ImGui::Text("パス:");
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
			if (ImGui::Button("エフェクトを保存"))
			{
				SaveEffect(effectPath_);
			}
			ImGui::PopStyleColor();
			
			ImGui::SameLine();
			ImGui::PushID(1234);
			if (ImGui::Button("名前を付けて保存..."))
			{
				effectPath_.clear();
				SaveEffect("");
			}
			ImGui::PopID();
		}
	}
}

void ParticleEditor::DrawPreviewPanel()
{
	if (!currentEffect_) return;

	if (ImGui::CollapsingHeader("プレビュー", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const size_t emitterCount = currentEffect_->GetEmitterCount();

		//===== 一括制御ボタン行 =====//
		// Play All: 全エミッターをRestart → エフェクトをPlay
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
		if (ImGui::Button("全部再生"))
		{
			for (size_t i = 0; i < emitterCount; ++i)
			{
				auto* emitter = currentEffect_->GetEmitter(i);
				if (emitter) emitter->Restart();
			}
			currentEffect_->Play();
			previewElapsed_ = 0.0f;
		}
		ImGui::PopStyleColor();

		ImGui::SameLine();

		// Stop All
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button("全部停止"))
		{
			currentEffect_->Stop();
			previewLooping_ = false;
		}
		ImGui::PopStyleColor();

		ImGui::SameLine();

		// Reset All: particles_もクリアして全エミッターを完全リセット → Play
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.1f, 1.0f));
		if (ImGui::Button("全部リセット"))
		{
			for (size_t i = 0; i < emitterCount; ++i)
			{
				auto* emitter = currentEffect_->GetEmitter(i);
				if (emitter) emitter->Reset();
			}
			currentEffect_->Play();
			previewElapsed_ = 0.0f;
		}
		ImGui::PopStyleColor();

		//===== ループ設定 =====//
		ImGui::Spacing();
		ImGui::Checkbox("プレビューを繰り返す", &previewLooping_);
		ImGui::SetItemTooltip("オン: 「全部再生」を一定間隔で繰り返す");

		if (previewLooping_)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.0f);
			ImGui::DragFloat("間隔 (秒)", &previewRepeatInterval_, 0.1f, 0.1f, 30.0f, "%.1f");

			// 残り時間バー
			float progress = (previewRepeatInterval_ > 0.0f)
				? previewElapsed_ / previewRepeatInterval_
				: 0.0f;
			ImGui::ProgressBar(progress, ImVec2(-1, 6));
		}

		//===== トレイルプレビュー設定 =====//
		ImGui::Spacing();
		ImGui::SeparatorText("トレイルの追従先プレビュー");
		ImGui::Checkbox("動くダミーを使う", &enablePreviewTarget_);
		ImGui::SetItemTooltip("オン: トレイルを試すための動くダミーを置く");
		if (enablePreviewTarget_)
		{
			ImGui::DragFloat("ダミーの速さ", &previewTargetSpeed_, 0.05f, 0.1f, 10.0f, "%.2f");
			ImGui::DragFloat("ダミーの半径", &previewTargetRadius_, 0.1f, 0.5f, 20.0f, "%.1f");
			ImGui::Text("ダミーの位置: (%.2f, %.2f, %.2f)", previewTargetPos_.x, previewTargetPos_.y, previewTargetPos_.z);
		}

		//===== エミッター一覧（パーティクル数表示）=====//
		ImGui::Spacing();
		ImGui::SeparatorText("エミッター");

		// 総パーティクル数
		uint32_t totalParticles = 0;
		for (size_t i = 0; i < emitterCount; ++i)
		{
			const auto* emitter = currentEffect_->GetEmitter(i);
			if (emitter) totalParticles += static_cast<uint32_t>(emitter->GetParticles().size());
		}
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "パーティクルの合計: %u", totalParticles);

		// エミッターごとの行
		ImGui::BeginChild("PreviewEmitterList", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
		for (size_t i = 0; i < emitterCount; ++i)
		{
			auto* emitter = currentEffect_->GetEmitter(i);
			if (!emitter) continue;

			ImGui::PushID(static_cast<int>(i) + 5000);

			// Emitting中か否かで色を変える
			bool isEmitting = emitter->IsEmitting();
			ImVec4 statusColor = isEmitting
				? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
				: ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

			ImGui::TextColored(statusColor, isEmitting ? "[ON] " : "[--] ");
			ImGui::SameLine();
			ImGui::Text("%-20s", emitter->GetName().c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("パーティクル数: %d", static_cast<int>(emitter->GetParticles().size()));

			// 行の右端にRestart/Stopボタン
			ImGui::SameLine(ImGui::GetWindowWidth() - 100.0f);
			if (ImGui::SmallButton("最初から"))
			{
				emitter->Restart();
				if (!currentEffect_->IsPlaying()) currentEffect_->Play();
				previewElapsed_ = 0.0f;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("停止"))
			{
				emitter->Stop();
			}

			ImGui::PopID();
		}

		if (emitterCount == 0)
		{
			ImGui::TextDisabled("  (No emitters)");
		}

		ImGui::EndChild();
	}
}

void ParticleEditor::DrawEmitterPanel()
{
	if (!currentEffect_) return;

	if (ImGui::CollapsingHeader("エミッター", ImGuiTreeNodeFlags_DefaultOpen))
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
				if (ImGui::MenuItem("削除"))
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
			if (ImGui::Button("選択中を削除"))
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
		if (emitter && ImGui::CollapsingHeader("エミッターの設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// Position
			Vector3 pos = emitter->GetPosition();
			if (ImGui::DragFloat3("位置", &pos.x, 0.1f))
			{
				emitter->SetPosition(pos);
			}

			// Max Particles
			int maxP = static_cast<int>(emitter->GetMaxParticles());
			if (ImGui::InputInt("最大パーティクル数", &maxP))
			{
				emitter->SetMaxParticles(static_cast<uint32_t>((std::max)(100, maxP)));
			}

			// Simulation Mode
			int mode = static_cast<int>(emitter->GetSimulationMode());
			const char* modes[] = { "CPU", "GPU" };
			if (ImGui::Combo("シミュレーション方式", &mode, modes, IM_ARRAYSIZE(modes)))
			{
				emitter->SetSimulationMode(static_cast<SimulationMode>(mode));
			}

			// Follow Offset
			Vector3 offset = emitter->GetFollowOffset();
			if (ImGui::DragFloat3("追従のずらし量##emitterSettings", &offset.x, 0.1f))
			{
				emitter->SetFollowOffset(offset);
			}
			ImGui::SetItemTooltip("Transform やエミッターに追従するときに足すずらし量");

			// Follow Emitter (同じエフェクト内の別エミッターを追従)
			ImGui::Separator();
			ImGui::Text("ほかのエミッターに追従:");
			int followIdx = emitter->GetFollowEmitterIndex();
			
			// エミッターリストを作成（現在選択中のエミッター以外）
			std::vector<const char*> emitterNames;
			std::vector<int> emitterIndices;
			emitterNames.push_back("なし");
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
			
			if (ImGui::Combo("追従するエミッター##emitterSettings", &currentSelection, emitterNames.data(), 
			                 static_cast<int>(emitterNames.size())))
			{
				emitter->SetFollowEmitterIndex(emitterIndices[currentSelection]);
			}
			ImGui::SetItemTooltip("このエフェクトの中で追従するエミッターを選ぶ");

			// Simulation Space
			const char* spaces[] = { "ワールド", "ローカル" };
			int space = static_cast<int>(emitter->GetSimulationSpace());
			if (ImGui::Combo("シミュレーション空間##emitterSettings", &space, spaces, 2))
			{
				emitter->SetSimulationSpace(static_cast<SimulationSpace>(space));
			}

			// 移動時のみ生成
			bool spawnOnlyWhenMoving = emitter->GetSpawnOnlyWhenMoving();
			if (ImGui::Checkbox("動いているときだけ出す", &spawnOnlyWhenMoving))
			{
				emitter->SetSpawnOnlyWhenMoving(spawnOnlyWhenMoving);
			}
			ImGui::SetItemTooltip("エミッターが動いているときだけパーティクルを出す（トレイル向き）");

			if (spawnOnlyWhenMoving)
			{
				float minDist = emitter->GetMinMoveDistance();
				if (ImGui::DragFloat("最小の移動距離", &minDist, 0.01f, 0.001f, 1.0f))
				{
					emitter->SetMinMoveDistance(minDist);
				}
				ImGui::SetItemTooltip("パーティクルを出すのに要るエミッターの移動距離");
			}

			//===== ライフサイクル設定 =====//
			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.4f, 1.0f));
			ImGui::Text("寿命");
			ImGui::PopStyleColor();

			// Duration
			float duration = emitter->GetDuration();
			if (ImGui::DragFloat("長さ", &duration, 0.1f, 0.0f, 100.0f))
			{
				emitter->SetDuration(duration);
				// エディタ用: 設定変更時に停止中なら再開
				if (!emitter->IsEmitting())
				{
					emitter->Reset();
				}
			}
			ImGui::SetItemTooltip("エミッターの寿命（秒）。0 で無限");

			// Start Delay
			float startDelay = emitter->GetStartDelay();
			if (ImGui::DragFloat("開始の遅れ", &startDelay, 0.1f, 0.0f, 10.0f))
			{
				emitter->SetStartDelay(startDelay);
			}
			ImGui::SetItemTooltip("パーティクルを出し始めるまでの遅れ");

			// Loop Behavior
			const char* loopBehaviors[] = { "1回", "無限", "回数指定" };
			int loopBehavior = static_cast<int>(emitter->GetLoopBehavior());
			if (ImGui::Combo("繰り返し方", &loopBehavior, loopBehaviors, 3))
			{
				emitter->SetLoopBehavior(static_cast<LoopBehavior>(loopBehavior));
				// エディタ用: 設定変更時に停止中なら再開
				if (!emitter->IsEmitting())
				{
					emitter->Reset();
				}
			}
			ImGui::SetItemTooltip("1回: 1回で止まる。無限: ずっと繰り返す。回数指定: N 回繰り返す。");

			// Loop Count (Multipleのときのみ表示)
			if (emitter->GetLoopBehavior() == LoopBehavior::Multiple)
			{
				int loopCount = emitter->GetLoopCount();
				if (ImGui::InputInt("繰り返し回数", &loopCount))
				{
					emitter->SetLoopCount((std::max)(1, loopCount));
					// エディタ用: 設定変更時に停止中なら再開
					if (!emitter->IsEmitting())
					{
						emitter->Reset();
					}
				}
			}

			// Inactive Response
			const char* inactiveResponses[] = { "最後まで", "すぐ消す" };
			int inactiveResponse = static_cast<int>(emitter->GetInactiveResponse());
			if (ImGui::Combo("止めたとき", &inactiveResponse, inactiveResponses, 2))
			{
				emitter->SetInactiveResponse(static_cast<InactiveResponse>(inactiveResponse));
			}
			ImGui::SetItemTooltip("最後まで: 出ているパーティクルは寿命まで残す。すぐ消す: すぐに消す。");

			// 状態表示 + リセットボタン
			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "経過: %.2f 秒", emitter->GetEmitterAge());
			ImGui::SameLine();
			ImGui::TextColored(
				emitter->IsEmitting() ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
				"放出中: %s", emitter->IsEmitting() ? "はい" : "いいえ");
			
			// 手動リセットボタン（停止中のみ表示）
			if (!emitter->IsEmitting())
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("最初から"))
				{
					emitter->Reset();
				}
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
	ImGui::Text("出ているパーティクル: %d", static_cast<int>(emitter->GetParticles().size()));
	ImGui::Separator();

	// モジュールリスト（Spawn/Updateで分離）
	if (ImGui::CollapsingHeader("モジュール", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int moduleToDelete = -1;

		// Spawnモジュール
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
		ImGui::SeparatorText("発生モジュール");
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
				if (ImGui::MenuItem("上へ") && i > 0) { emitter->MoveModuleUp(i); }
				if (ImGui::MenuItem("下へ") && i < emitter->GetModuleCount() - 1) { emitter->MoveModuleDown(i); }
				ImGui::Separator();
				if (ImGui::MenuItem("削除")) { moduleToDelete = static_cast<int>(i); }
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
		ImGui::SeparatorText("更新モジュール");
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
				if (ImGui::MenuItem("上へ") && i > 0) { emitter->MoveModuleUp(i); }
				if (ImGui::MenuItem("下へ") && i < emitter->GetModuleCount() - 1) { emitter->MoveModuleDown(i); }
				ImGui::Separator();
				if (ImGui::MenuItem("削除")) { moduleToDelete = static_cast<int>(i); }
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

	auto DrawEasingCombo = [](const char* label, EasingType* type) {
		const char* easingNames[] = {
			"Linear", "EaseInSine", "EaseOutSine", "EaseInOutSine",
			"EaseInQuad", "EaseOutQuad", "EaseInOutQuad",
			"EaseInCubic", "EaseOutCubic", "EaseInOutCubic",
			"EaseInQuart", "EaseOutQuart", "EaseInOutQuart"
		};
		int current = static_cast<int>(*type);
		if (ImGui::Combo(label, &current, easingNames, IM_ARRAYSIZE(easingNames))) {
			*type = static_cast<EasingType>(current);
			return true;
		}
		return false;
	};

	ImGui::Text("モジュール: %s", module->GetName());
	ImGui::Separator();

	// 各モジュールタイプごとにキャスト
	if (auto* m = dynamic_cast<SpawnRateModule*>(module))
	{
		float rate = m->GetRate();
		if (ImGui::DragFloat("1秒あたりの数", &rate, 0.1f, 0.0f, 1000.0f))
		{
			m->SetRate(rate);
		}
	}
	else if (auto* m = dynamic_cast<SpawnBurstModule*>(module))
	{
		int count = static_cast<int>(m->GetCount());
		if (ImGui::InputInt("数", &count)) { m->SetCount(static_cast<uint32_t>((std::max)(0, count))); }

		float delay = m->GetDelay();
		if (ImGui::DragFloat("遅れ", &delay, 0.01f, 0.0f, 10.0f)) { m->SetDelay(delay); }

		float interval = m->GetInterval();
		if (ImGui::DragFloat("間隔", &interval, 0.01f, 0.0f, 10.0f)) { m->SetInterval(interval); }

		int loops = m->GetLoops();
		if (ImGui::InputInt("繰り返し (-1で無限)", &loops)) { m->SetLoops(loops); }
	}
	else if (auto* m = dynamic_cast<InitialLifetimeModule*>(module))
	{
		float min = m->GetMinLifetime();
		float max = m->GetMaxLifetime();
		if (ImGui::DragFloat("寿命の最小", &min, 0.01f, 0.01f, 100.0f)) { m->SetMinLifetime(min); }
		if (ImGui::DragFloat("寿命の最大", &max, 0.01f, 0.01f, 100.0f)) { m->SetMaxLifetime(max); }
	}
	else if (auto* m = dynamic_cast<InitialVelocityModule*>(module))
	{
		Vector3 min = m->GetMinVelocity();
		Vector3 max = m->GetMaxVelocity();
		if (ImGui::DragFloat3("速度の最小", &min.x, 0.1f)) { m->SetMinVelocity(min); }
		if (ImGui::DragFloat3("速度の最大", &max.x, 0.1f)) { m->SetMaxVelocity(max); }
	}
	else if (auto* m = dynamic_cast<InitialScaleModule*>(module))
	{
		Vector3 min = m->GetMinScale();
		Vector3 max = m->GetMaxScale();
		if (ImGui::DragFloat3("スケールの最小", &min.x, 0.01f)) { m->SetMinScale(min); }
		if (ImGui::DragFloat3("スケールの最大", &max.x, 0.01f)) { m->SetMaxScale(max); }
	}
	else if (auto* m = dynamic_cast<InitialColorModule*>(module))
	{
		Vector4 min = m->GetMinColor();
		Vector4 max = m->GetMaxColor();
		if (ImGui::ColorEdit4("色の最小", &min.x)) { m->SetMinColor(min); }
		if (ImGui::ColorEdit4("色の最大", &max.x)) { m->SetMaxColor(max); }
	}
	else if (auto* m = dynamic_cast<AssignRibbonIdModule*>(module))
	{
		int groupCount = static_cast<int>(m->GetGroupCount());
		if (ImGui::InputInt("グループ数", &groupCount, 1, 5))
		{
			m->SetGroupCount(static_cast<uint32_t>((std::max)(1, groupCount)));
		}
		ImGui::SetItemTooltip("リボンのグループ数。グループごとに別のトレイルになる。");
	}
	else if (auto* m = dynamic_cast<GravityModule*>(module))
	{
		Vector3 minG = m->GetMinGravity();
		Vector3 maxG = m->GetMaxGravity();
		if (ImGui::DragFloat3("重力の最小", &minG.x, 0.1f)) { m->SetGravityRange(minG, maxG); }
		if (ImGui::DragFloat3("重力の最大", &maxG.x, 0.1f)) { m->SetGravityRange(minG, maxG); }
	}
	else if (auto* m = dynamic_cast<DragModule*>(module))
	{
		float minD = m->GetMinDrag();
		float maxD = m->GetMaxDrag();
		if (ImGui::DragFloat("空気抵抗の最小", &minD, 0.01f, 0.0f, 10.0f)) { m->SetDragRange(minD, maxD); }
		if (ImGui::DragFloat("空気抵抗の最大", &maxD, 0.01f, 0.0f, 10.0f)) { m->SetDragRange(minD, maxD); }
	}
	else if (auto* m = dynamic_cast<ColorFadeModule*>(module))
	{
		bool useInitial = m->GetUseInitialColor();
		if (ImGui::Checkbox("初期色から始める", &useInitial)) 
		{ 
			m->SetUseInitialColor(useInitial); 
		}
		ImGui::SetItemTooltip("オンにすると、開始色ではなく InitialColor モジュールの色から変える");
		
		if (!useInitial)
		{
			Vector4 start = m->GetStartColor();
			if (ImGui::ColorEdit4("開始色", &start.x)) { m->SetStartColor(start); }
		}
		Vector4 end = m->GetEndColor();
		if (ImGui::ColorEdit4("終了色", &end.x)) { m->SetEndColor(end); }

		EasingType easing = m->GetEasingType();
		if (DrawEasingCombo("イージング", &easing)) { m->SetEasingType(easing); }
	}
	else if (auto* m = dynamic_cast<ScaleOverLifetimeModule*>(module))
	{
		Vector3 start = m->GetStartScale();
		Vector3 end = m->GetEndScale();
		if (ImGui::DragFloat3("開始のスケール", &start.x, 0.01f)) { m->SetStartScale(start); }
		if (ImGui::DragFloat3("終了のスケール", &end.x, 0.01f)) { m->SetEndScale(end); }

		EasingType easing = m->GetEasingType();
		if (DrawEasingCombo("イージング", &easing)) { m->SetEasingType(easing); }
	}
	else if (auto* m = dynamic_cast<TextureSheetModule*>(module))
	{
		int cols = static_cast<int>(m->GetColumns());
		int rows = static_cast<int>(m->GetRows());
		if (ImGui::InputInt("列", &cols)) { m->SetGridSize(static_cast<uint32_t>((std::max)(1, cols)), m->GetRows()); }
		if (ImGui::InputInt("行", &rows)) { m->SetGridSize(m->GetColumns(), static_cast<uint32_t>((std::max)(1, rows))); }

		float fps = m->GetFrameRate();
		if (ImGui::DragFloat("フレームレート", &fps, 0.1f, 0.1f, 120.0f)) { m->SetFrameRate(fps); }

		const char* playModes[] = { "ループ", "1回", "往復" };
		int mode = static_cast<int>(m->GetPlayMode());
		if (ImGui::Combo("再生の仕方", &mode, playModes, 3)) { m->SetPlayMode(static_cast<TextureSheetPlayMode>(mode)); }
	}
	else if (auto* m = dynamic_cast<AttractorModule*>(module))
	{
		Vector3 target = m->GetTarget();
		if (ImGui::DragFloat3("目標", &target.x, 0.1f)) { m->SetTarget(target); }

		float strength = m->GetStrength();
		if (ImGui::DragFloat("強さ", &strength, 0.1f)) { m->SetStrength(strength); }

		float range = m->GetRange();
		if (ImGui::DragFloat("範囲", &range, 0.1f, 0.0f, 100.0f)) { m->SetRange(range); }

		const char* falloffTypes[] = { "なし", "直線", "距離の2乗に反比例" };
		int falloff = static_cast<int>(m->GetFalloffType());
		if (ImGui::Combo("減衰", &falloff, falloffTypes, 3)) { m->SetFalloffType(static_cast<FalloffType>(falloff)); }
	}
	else if (auto* m = dynamic_cast<VortexModule*>(module))
	{
		Vector3 axis = m->GetAxis();
		Vector3 center = m->GetCenter();
		if (ImGui::DragFloat3("軸", &axis.x, 0.01f)) { m->SetAxis(axis); }
		if (ImGui::DragFloat3("中心", &center.x, 0.1f)) { m->SetCenter(center); }

		float strength = m->GetStrength();
		if (ImGui::DragFloat("強さ", &strength, 0.1f)) { m->SetStrength(strength); }

		float range = m->GetRange();
		if (ImGui::DragFloat("範囲", &range, 0.1f, 0.0f, 100.0f)) { m->SetRange(range); }
	}
	else if (auto* m = dynamic_cast<SpawnShapeModule*>(module))
	{
		const char* shapeTypes[] = { "Point", "Sphere", "Circle", "Box", "Cone", "Line" };
		int shape = static_cast<int>(m->GetShapeType());
		if (ImGui::Combo("形", &shape, shapeTypes, 6))
		{
			m->SetShapeType(static_cast<SpawnShapeType>(shape));
		}

		// SpawnLocation (Volume/Surface/Edge)
		SpawnShapeType currentShape = m->GetShapeType();
		if (currentShape == SpawnShapeType::Sphere || currentShape == SpawnShapeType::Circle || 
		    currentShape == SpawnShapeType::Box)
		{
			const char* spawnLocations[] = { "内部", "表面", "辺" };
			int loc = static_cast<int>(m->GetSpawnLocation());
			if (ImGui::Combo("出す場所", &loc, spawnLocations, 
			    (currentShape == SpawnShapeType::Box) ? 3 : 2)) // Edge only for Box
			{
				m->SetSpawnLocation(static_cast<SpawnLocation>(loc));
			}
			ImGui::SetItemTooltip("内部: 中を埋める。表面: 外側だけ。辺: 箱の辺だけ");
		}

		float innerRadius = m->GetInnerRadius();
		float outerRadius = m->GetOuterRadius();
		if (ImGui::DragFloat("内側の半径", &innerRadius, 0.1f, 0.0f, 100.0f)) { m->SetInnerRadius(innerRadius); }
		if (ImGui::DragFloat("外側の半径", &outerRadius, 0.1f, 0.0f, 100.0f)) { m->SetOuterRadius(outerRadius); }

		Vector3 boxSize = m->GetBoxSize();
		if (ImGui::DragFloat3("箱の大きさ", &boxSize.x, 0.1f)) { m->SetBoxSize(boxSize); }

		float coneHeight = m->GetConeHeight();
		if (ImGui::DragFloat("円錐の高さ", &coneHeight, 0.1f, 0.0f, 100.0f)) { m->SetConeHeight(coneHeight); }

		bool emitFromSurface = m->GetEmitFromSurface();
		if (ImGui::Checkbox("表面から出す", &emitFromSurface)) { m->SetEmitFromSurface(emitFromSurface); }

		float initialSpeed = m->GetInitialSpeed();
		if (ImGui::DragFloat("初速", &initialSpeed, 0.1f, 0.0f, 100.0f)) { m->SetInitialSpeed(initialSpeed); }

		// Line用パラメータ
		Vector3 lineStart = m->GetLineStart();
		Vector3 lineEnd = m->GetLineEnd();
		if (ImGui::DragFloat3("線の始点", &lineStart.x, 0.1f)) { m->SetLine(lineStart, lineEnd); }
		if (ImGui::DragFloat3("線の終点", &lineEnd.x, 0.1f)) { m->SetLine(lineStart, lineEnd); }

		float arcAngle = m->GetArcAngle();
		if (ImGui::DragFloat("弧の角度", &arcAngle, 1.0f, 0.0f, 360.0f)) { m->SetArcAngle(arcAngle); }
		ImGui::SetItemTooltip("Circle と Cone で出す弧の角度（0〜360）");
	}
	else if (auto* m = dynamic_cast<InitialRotationModule*>(module))
	{
		Vector3 minAngle = m->GetMinAngle();
		Vector3 maxAngle = m->GetMaxAngle();
		if (ImGui::DragFloat3("角度の最小", &minAngle.x, 1.0f, 0.0f, 360.0f)) { m->SetRotationRange(minAngle, maxAngle); }
		if (ImGui::DragFloat3("角度の最大", &maxAngle.x, 1.0f, 0.0f, 360.0f)) { m->SetRotationRange(minAngle, maxAngle); }
	}
	else if (auto* m = dynamic_cast<RotationOverLifetimeModule*>(module))
	{
		float startSpeed = m->GetStartSpeed();
		float endSpeed = m->GetEndSpeed();
		if (ImGui::DragFloat("開始の回転速度 (度/秒)", &startSpeed, 1.0f, -1000.0f, 1000.0f)) { m->SetRotationSpeedRange(startSpeed, endSpeed); }
		if (ImGui::DragFloat("終了の回転速度 (度/秒)", &endSpeed, 1.0f, -1000.0f, 1000.0f)) { m->SetRotationSpeedRange(startSpeed, endSpeed); }

		EasingType easing = m->GetEasingType();
		if (DrawEasingCombo("イージング", &easing)) { m->SetEasingType(easing); }
	}
	else if (auto* m = dynamic_cast<FaceVelocityModule*>(module))
	{
		ImGui::Text("パーティクルの向きを進む方向に合わせる。");
		bool use2D = m->IsUse2DAlignment();
		if (ImGui::Checkbox("2D で合わせる", &use2D))
		{
			m->SetUse2DAlignment(use2D);
		}
		ImGui::SetItemTooltip("オン: 2D の回転（スプライト向け）。オフ: 3D のピッチ/ヨー（メッシュ向け）");
	}
	else if (auto* m = dynamic_cast<JitterModule*>(module))
	{
		Vector3 amount = m->GetAmount();
		if (ImGui::DragFloat3("量", &amount.x, 0.01f, 0.0f, 10.0f)) { m->SetAmount(amount); }
	}
	else if (auto* m = dynamic_cast<ForceOverLifetimeModule*>(module))
	{
		Vector3 dir = m->GetDirection();
		if (ImGui::DragFloat3("向き", &dir.x, 0.1f)) { m->SetDirection(dir); }

		float startS = m->GetStartStrength();
		float endS = m->GetEndStrength();
		if (ImGui::DragFloat("開始の強さ", &startS, 0.1f)) { m->SetStrengths(startS, endS); }
		if (ImGui::DragFloat("終了の強さ", &endS, 0.1f)) { m->SetStrengths(startS, endS); }

		EasingType easing = m->GetEasingType();
		if (DrawEasingCombo("イージング", &easing)) { m->SetEasingType(easing); }
	}
	else if (auto* m = dynamic_cast<OrbitModule*>(module))
	{
		float speed = m->GetOrbitSpeed();
		if (ImGui::DragFloat("周回の速さ (度/秒)", &speed, 1.0f, -360.0f, 360.0f)) { m->SetOrbitSpeed(speed); }

		Vector3 axis = m->GetOrbitAxis();
		if (ImGui::DragFloat3("周回の軸", &axis.x, 0.01f)) { m->SetOrbitAxis(axis); }
	}
	else if (auto* m = dynamic_cast<NoiseModule*>(module))
	{
		float strength = m->GetStrength();
		float frequency = m->GetFrequency();
		if (ImGui::DragFloat("強さ", &strength, 0.1f, 0.0f, 100.0f)) { m->SetStrength(strength); }
		if (ImGui::DragFloat("周波数", &frequency, 0.1f, 0.0f, 10.0f)) { m->SetFrequency(frequency); }
	}
	else if (auto* m = dynamic_cast<VelocityLimitModule*>(module))
	{
		float maxSpeed = m->GetMaxSpeed();
		if (ImGui::DragFloat("最大速度", &maxSpeed, 0.1f, 0.0f, 100.0f)) { m->SetMaxSpeed(maxSpeed); }
	}
	// Phase 3: New modules
	else if (auto* m = dynamic_cast<AccelerationModule*>(module))
	{
		Vector3 acc = m->GetAcceleration();
		if (ImGui::DragFloat3("加速度", &acc.x, 0.1f)) { m->SetAcceleration(acc); }
	}
	else if (auto* m = dynamic_cast<CurlNoiseModule*>(module))
	{
		float strength = m->GetStrength();
		if (ImGui::DragFloat("強さ", &strength, 0.1f, 0.0f, 50.0f)) { m->SetStrength(strength); }
		
		float frequency = m->GetFrequency();
		if (ImGui::DragFloat("周波数", &frequency, 0.1f, 0.01f, 10.0f)) { m->SetFrequency(frequency); }
		
		int octaves = m->GetOctaves();
		if (ImGui::SliderInt("オクターブ", &octaves, 1, 8)) { m->SetOctaves(octaves); }
		
		float scroll = m->GetScrollSpeed();
		if (ImGui::DragFloat("流れる速さ", &scroll, 0.1f, 0.0f, 10.0f)) { m->SetScrollSpeed(scroll); }
	}
	else if (auto* m = dynamic_cast<SizeBySpeedModule*>(module))
	{
		float minSpd = m->GetMinSpeed();
		float maxSpd = m->GetMaxSpeed();
		if (ImGui::DragFloat("最小速度", &minSpd, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpd, maxSpd); }
		if (ImGui::DragFloat("最大速度", &maxSpd, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpd, maxSpd); }
		
		Vector3 minScale = m->GetMinScale();
		Vector3 maxScale = m->GetMaxScale();
		if (ImGui::DragFloat3("スケールの最小", &minScale.x, 0.01f)) { m->SetScaleRange(minScale, maxScale); }
		if (ImGui::DragFloat3("スケールの最大", &maxScale.x, 0.01f)) { m->SetScaleRange(minScale, maxScale); }
	}
	else if (auto* m = dynamic_cast<ColorBySpeedModule*>(module))
	{
		float minSpd = m->GetMinSpeed();
		float maxSpd = m->GetMaxSpeed();
		if (ImGui::DragFloat("最小速度", &minSpd, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpd, maxSpd); }
		if (ImGui::DragFloat("最大速度", &maxSpd, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpd, maxSpd); }
		
		Vector4 minColor = m->GetMinColor();
		Vector4 maxColor = m->GetMaxColor();
		if (ImGui::ColorEdit4("色の最小", &minColor.x)) { m->SetColorRange(minColor, maxColor); }
		if (ImGui::ColorEdit4("色の最大", &maxColor.x)) { m->SetColorRange(minColor, maxColor); }
	}
	else if (auto* m = dynamic_cast<CollisionModule*>(module))
	{
		const char* modes[] = { "平面", "ワールド", "箱" };
		int mode = static_cast<int>(m->GetMode());
		if (ImGui::Combo("方式", &mode, modes, 3)) { m->SetMode(static_cast<CollisionMode>(mode)); }
		
		float bounce = m->GetBounce();
		if (ImGui::DragFloat("跳ね返り", &bounce, 0.01f, 0.0f, 1.0f)) { m->SetBounce(bounce); }
		
		float friction = m->GetFriction();
		if (ImGui::DragFloat("摩擦", &friction, 0.01f, 0.0f, 1.0f)) { m->SetFriction(friction); }
		
		if (m->GetMode() == CollisionMode::Plane || m->GetMode() == CollisionMode::World)
		{
			float height = m->GetPlaneHeight();
			if (ImGui::DragFloat("平面の高さ", &height, 0.1f)) { m->SetPlaneHeight(height); }
		}
		else if (m->GetMode() == CollisionMode::Box)
		{
			Vector3 center = m->GetBoxCenter();
			Vector3 size = m->GetBoxSize();
			if (ImGui::DragFloat3("箱の中心", &center.x, 0.1f)) { m->SetBoxCenter(center); }
			if (ImGui::DragFloat3("箱の大きさ", &size.x, 0.1f)) { m->SetBoxSize(size); }
		}
		
		bool killOnCol = m->GetKillOnCollision();
		if (ImGui::Checkbox("当たったら消す", &killOnCol)) { m->SetKillOnCollision(killOnCol); }
	}
	else if (auto* m = dynamic_cast<KillZoneModule*>(module))
	{
		const char* zoneTypes[] = { "箱", "球" };
		int zType = static_cast<int>(m->GetZoneType());
		if (ImGui::Combo("範囲の形", &zType, zoneTypes, 2)) { m->SetZoneType(static_cast<KillZoneType>(zType)); }
		
		Vector3 center = m->GetCenter();
		if (ImGui::DragFloat3("中心", &center.x, 0.1f)) { m->SetCenter(center); }
		
		if (m->GetZoneType() == KillZoneType::Box)
		{
			Vector3 size = m->GetBoxSize();
			if (ImGui::DragFloat3("大きさ", &size.x, 0.1f)) { m->SetBoxSize(size); }
		}
		else
		{
			float radius = m->GetRadius();
			if (ImGui::DragFloat("半径", &radius, 0.1f, 0.0f, 100.0f)) { m->SetRadius(radius); }
		}
		
		bool killInside = m->GetKillInside();
		if (ImGui::Checkbox("内側で消す", &killInside)) { m->SetKillInside(killInside); }
		ImGui::SetItemTooltip("オン: 範囲の内側で消す。オフ: 外側で消す");
	}
	else if (auto* m = dynamic_cast<SprintToTargetModule*>(module))
	{
		Vector3 target = m->GetTarget();
		if (ImGui::DragFloat3("目標", &target.x, 0.1f)) { m->SetTarget(target); }
		
		float acc = m->GetAcceleration();
		if (ImGui::DragFloat("加速度", &acc, 0.1f, 0.0f, 50.0f)) { m->SetAcceleration(acc); }
		
		float arriveRad = m->GetArriveRadius();
		if (ImGui::DragFloat("到着とみなす半径", &arriveRad, 0.1f, 0.0f, 10.0f)) { m->SetArriveRadius(arriveRad); }
		
		bool killOnArrive = m->GetKillOnArrive();
		if (ImGui::Checkbox("着いたら消す", &killOnArrive)) { m->SetKillOnArrive(killOnArrive); }
		
		bool useSpeedCurve = m->GetUseSpeedCurve();
		if (ImGui::Checkbox("距離で速さを変える", &useSpeedCurve)) { m->SetUseSpeedCurve(useSpeedCurve); }
		
		if (useSpeedCurve)
		{
			float maxDist = m->GetMaxDistance();
			if (ImGui::DragFloat("最大距離", &maxDist, 0.1f, 1.0f, 100.0f)) { m->SetMaxDistance(maxDist); }
			
			float speedBoost = m->GetSpeedBoost();
			if (ImGui::DragFloat("速さの上乗せ", &speedBoost, 0.1f, 0.0f, 10.0f)) { m->SetSpeedBoost(speedBoost); }
		}
	}
	// Phase 4: Sub-Emitters
	else if (auto* m = dynamic_cast<SubEmitterModule*>(module))
	{
		ImGui::Text("サブエミッターの設定: %d", static_cast<int>(m->GetConfigCount()));
		
		// 既存設定の編集
		for (size_t i = 0; i < m->GetConfigCount(); ++i)
		{
			auto* config = m->GetConfig(i);
			if (!config) continue;
			
			ImGui::PushID(static_cast<int>(i));
			ImGui::Separator();
			ImGui::Text("設定 %d", static_cast<int>(i + 1));
			
			// Effect Path
			static char pathBuffer[256];
			strncpy_s(pathBuffer, config->effectPath.c_str(), sizeof(pathBuffer) - 1);
			if (ImGui::InputText("エフェクトのパス", pathBuffer, sizeof(pathBuffer)))
			{
				config->effectPath = pathBuffer;
			}
			ImGui::SetItemTooltip("サブエフェクトの JSON ファイルのパス");
			
			// Trigger
			const char* triggers[] = { "OnSpawn", "OnDeath", "OnCollision", "Continuous" };
			int trigger = static_cast<int>(config->trigger);
			if (ImGui::Combo("きっかけ", &trigger, triggers, 4))
			{
				config->trigger = static_cast<SubEmitterTrigger>(trigger);
			}
			
			// Probability
			if (ImGui::DragFloat("確率", &config->probability, 0.01f, 0.0f, 1.0f))
			{
				config->probability = (std::clamp)(config->probability, 0.0f, 1.0f);
			}
			
			// Continuous rate (only for Continuous trigger)
			if (config->trigger == SubEmitterTrigger::Continuous)
			{
				ImGui::DragFloat("1秒あたりの数", &config->continuousRate, 0.1f, 0.1f, 100.0f);
			}
			
			// Inheritance settings
			ImGui::Checkbox("位置を引き継ぐ", &config->inheritPosition);
			ImGui::Checkbox("速度を引き継ぐ", &config->inheritVelocity);
			if (config->inheritVelocity)
			{
				ImGui::DragFloat("速度の倍率", &config->inheritVelocityScale, 0.1f, 0.0f, 2.0f);
			}
			ImGui::Checkbox("色を引き継ぐ", &config->inheritColor);
			ImGui::Checkbox("スケールを引き継ぐ", &config->inheritScale);
			
			// Delete button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("設定を削除"))
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
	// Motion Effect Modules - Phase 5
	else if (auto* m = dynamic_cast<RadialVelocityModule*>(module))
	{
		float minSpeed = m->GetMinSpeed();
		float maxSpeed = m->GetMaxSpeed();
		if (ImGui::DragFloat("最小速度", &minSpeed, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpeed, maxSpeed); }
		if (ImGui::DragFloat("最大速度", &maxSpeed, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpeed, maxSpeed); }
	}
	else if (auto* m = dynamic_cast<VelocityOverLifetimeModule*>(module))
	{
		float startMul = m->GetStartMultiplier();
		float endMul = m->GetEndMultiplier();
		if (ImGui::DragFloat("開始の倍率", &startMul, 0.01f, 0.0f, 2.0f)) { m->SetStartMultiplier(startMul); }
		if (ImGui::DragFloat("終了の倍率", &endMul, 0.01f, 0.0f, 2.0f)) { m->SetEndMultiplier(endMul); }
	}
	else if (auto* m = dynamic_cast<StretchByVelocityModule*>(module))
	{
		float factor = m->GetStretchFactor();
		if (ImGui::DragFloat("伸びの強さ", &factor, 0.01f, 0.0f, 1.0f)) { m->SetStretchFactor(factor); }
		
		float minS = m->GetMinStretch();
		float maxS = m->GetMaxStretch();
		if (ImGui::DragFloat("伸びの最小", &minS, 0.1f, 0.1f, 10.0f)) { m->SetMinStretch(minS); }
		if (ImGui::DragFloat("伸びの最大", &maxS, 0.1f, 0.1f, 10.0f)) { m->SetMaxStretch(maxS); }
		
		bool preserve = m->GetPreserveVolume();
		if (ImGui::Checkbox("体積を保つ", &preserve)) { m->SetPreserveVolume(preserve); }
		ImGui::SetItemTooltip("Y に伸ばすときに X/Z を縮めて体積を保つ");
	}
	else if (auto* m = dynamic_cast<WindModule*>(module))
	{
		Vector3 dir = m->GetDirection();
		if (ImGui::DragFloat3("向き", &dir.x, 0.01f)) { m->SetDirection(dir); }
		
		float strength = m->GetStrength();
		if (ImGui::DragFloat("強さ", &strength, 0.1f, 0.0f, 50.0f)) { m->SetStrength(strength); }
		
		float turb = m->GetTurbulence();
		if (ImGui::DragFloat("乱れ", &turb, 0.01f, 0.0f, 1.0f)) { m->SetTurbulence(turb); }
		
		float turbFreq = m->GetTurbulenceFrequency();
		if (ImGui::DragFloat("乱れの周波数", &turbFreq, 0.1f, 0.1f, 10.0f)) { m->SetTurbulenceFrequency(turbFreq); }
	}
	else if (auto* m = dynamic_cast<FlickerModule*>(module))
	{
		float freq = m->GetFrequency();
		if (ImGui::DragFloat("周波数", &freq, 0.1f, 0.1f, 50.0f)) { m->SetFrequency(freq); }
		
		float minA = m->GetMinAlpha();
		float maxA = m->GetMaxAlpha();
		if (ImGui::DragFloat("アルファの最小", &minA, 0.01f, 0.0f, 1.0f)) { m->SetMinAlpha(minA); }
		if (ImGui::DragFloat("アルファの最大", &maxA, 0.01f, 0.0f, 1.0f)) { m->SetMaxAlpha(maxA); }
		
		bool randPhase = m->GetRandomPhase();
		if (ImGui::Checkbox("位相をばらつかせる", &randPhase)) { m->SetRandomPhase(randPhase); }
		
		bool useNoise = m->GetUseNoise();
		if (ImGui::Checkbox("ノイズを使う", &useNoise)) { m->SetUseNoise(useNoise); }
	}
	else if (auto* m = dynamic_cast<AlphaFadeModule*>(module))
	{
		float startA = m->GetStartAlpha();
		float endA = m->GetEndAlpha();
		if (ImGui::DragFloat("開始のアルファ", &startA, 0.01f, 0.0f, 1.0f)) { m->SetStartAlpha(startA); }
		if (ImGui::DragFloat("終了のアルファ", &endA, 0.01f, 0.0f, 1.0f)) { m->SetEndAlpha(endA); }
		
		bool easeIn = m->GetEaseIn();
		bool easeOut = m->GetEaseOut();
		if (ImGui::Checkbox("イーズイン", &easeIn)) { m->SetEaseIn(easeIn); }
		ImGui::SameLine();
		if (ImGui::Checkbox("イーズアウト", &easeOut)) { m->SetEaseOut(easeOut); }
	}
	else if (auto* m = dynamic_cast<RotationBySpeedModule*>(module))
	{
		float rotPerSpeed = m->GetRotationPerSpeed();
		if (ImGui::DragFloat("速さあたりの回転 (度)", &rotPerSpeed, 1.0f, -360.0f, 360.0f)) { m->SetRotationPerSpeed(rotPerSpeed); }
		
		float minSpd = m->GetMinSpeed();
		float maxSpd = m->GetMaxSpeed();
		if (ImGui::DragFloat("最小速度", &minSpd, 0.1f, 0.0f, 100.0f)) { m->SetMinSpeed(minSpd); }
		if (ImGui::DragFloat("最大速度 (0で無制限)", &maxSpd, 0.1f, 0.0f, 100.0f)) { m->SetMaxSpeed(maxSpd); }
	}
	else if (auto* m = dynamic_cast<SineWaveModule*>(module))
	{
		float amp = m->GetAmplitude();
		if (ImGui::DragFloat("振幅", &amp, 0.1f, 0.0f, 10.0f)) { m->SetAmplitude(amp); }
		
		float freq = m->GetFrequency();
		if (ImGui::DragFloat("周波数", &freq, 0.1f, 0.1f, 20.0f)) { m->SetFrequency(freq); }
		
		Vector3 axis = m->GetAxis();
		if (ImGui::DragFloat3("軸", &axis.x, 0.01f)) { m->SetAxis(axis); }
		
		bool randPhase = m->GetRandomPhase();
		if (ImGui::Checkbox("位相をばらつかせる", &randPhase)) { m->SetRandomPhase(randPhase); }
	}
	else if (auto* m = dynamic_cast<SpiralModule*>(module))
	{
		float radius = m->GetRadius();
		if (ImGui::DragFloat("半径", &radius, 0.1f, 0.01f, 10.0f)) { m->SetRadius(radius); }
		
		float speed = m->GetSpeed();
		if (ImGui::DragFloat("速さ (度/秒)", &speed, 1.0f, -720.0f, 720.0f)) { m->SetSpeed(speed); }
		
		float lift = m->GetLift();
		if (ImGui::DragFloat("上昇", &lift, 0.1f, -10.0f, 10.0f)) { m->SetLift(lift); }
		
		bool randPhase = m->GetRandomPhase();
		if (ImGui::Checkbox("位相をばらつかせる", &randPhase)) { m->SetRandomPhase(randPhase); }
		
		bool expand = m->GetExpandRadius();
		if (ImGui::Checkbox("半径を広げる", &expand)) { m->SetExpandRadius(expand); }
		
		if (expand)
		{
			float expRate = m->GetExpansionRate();
			if (ImGui::DragFloat("広がる速さ", &expRate, 0.1f, 0.0f, 5.0f)) { m->SetExpansionRate(expRate); }
		}
	}
	else if (auto* m = dynamic_cast<TwistModule*>(module))
	{
		float twistSpeed = m->GetTwistSpeed();
		if (ImGui::DragFloat("ねじれの速さ (度/秒)", &twistSpeed, 1.0f, -360.0f, 360.0f)) { m->SetTwistSpeed(twistSpeed); }
		
		float twistStrength = m->GetTwistStrength();
		if (ImGui::DragFloat("ねじれの強さ", &twistStrength, 0.1f, 0.0f, 10.0f)) { m->SetTwistStrength(twistStrength); }
		
		const char* axes[] = { "X", "Y", "Z" };
		int axis = m->GetHeightAxis();
		if (ImGui::Combo("高さの軸", &axis, axes, 3)) { m->SetHeightAxis(axis); }
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
	ImGui::SeparatorText("レンダラー");
	ImGui::PopStyleColor();
	
	{
		auto* renderer = emitter->GetRenderer();
		if (renderer)
		{
			const char* typeNames[] = { "Sprite", "Trail", "Mesh" };
			int currentType = static_cast<int>(renderer->GetType());
			if (ImGui::Combo("種類", &currentType, typeNames, 3))
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

			const char* blendModes[] = {
				"Alpha",
				"Additive",
				"Subtractive",
				"Multiply",
				"Screen",
				"Darken",
				"Lighten",
				"ColorBurn",
				"ColorDodge"
			};
			int currentBlend = static_cast<int>(renderer->GetBlendMode());
			if (ImGui::Combo("ブレンドモード", &currentBlend, blendModes, IM_ARRAYSIZE(blendModes)))
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
					ImGui::Text("テクスチャ:");

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
							if (displayName.empty()) displayName = "不明";

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

				// ティントカラー
				Vector4 tintColor = spriteRenderer->GetTintColor();
				if (ImGui::ColorEdit4("色味##Sprite", &tintColor.x))
				{
					spriteRenderer->SetTintColor(tintColor);
				}
			}
			// Trail Renderer特有の設定
			else if (auto* trailRenderer = dynamic_cast<TrailRenderer*>(renderer))
			{
				ImGui::Separator();
				ImGui::Text("トレイルの設定:");

				// トレイル幅
				float width = trailRenderer->GetTrailWidth();
				if (ImGui::DragFloat("トレイルの太さ", &width, 0.01f, 0.01f, 10.0f))
				{
					trailRenderer->SetTrailWidth(width);
				}

				// トレイル寿命
				float trailLifetime = trailRenderer->GetTrailLifetime();
				if (ImGui::DragFloat("トレイルの寿命", &trailLifetime, 0.1f, 0.1f, 10.0f))
				{
					trailRenderer->SetTrailLifetime(trailLifetime);
				}
				ImGui::SetItemTooltip("トレイルが残る時間（秒）");

				// 記録間隔
				float recordInterval = trailRenderer->GetRecordInterval();
				if (ImGui::DragFloat("記録の間隔", &recordInterval, 0.001f, 0.001f, 0.1f))
				{
					trailRenderer->SetRecordInterval(recordInterval);
				}
				ImGui::SetItemTooltip("位置を記録する間隔（小さいほどなめらか）");

				// 最小セグメント距離
				float minDist = trailRenderer->GetMinSegmentDistance();
				if (ImGui::DragFloat("区間の最小距離", &minDist, 0.01f, 0.01f, 1.0f))
				{
					trailRenderer->SetMinSegmentDistance(minDist);
				}

				// テクスチャモード
				const char* textureModes[] = { "引き伸ばし", "繰り返し" };
				int texMode = static_cast<int>(trailRenderer->GetTextureMode());
				if (ImGui::Combo("テクスチャの貼り方", &texMode, textureModes, 2))
				{
					trailRenderer->SetTextureMode(static_cast<RibbonTextureMode>(texMode));
				}

				// タイルスケール（Tileモードのみ）
				if (trailRenderer->GetTextureMode() == RibbonTextureMode::Tile)
				{
					float tileScale = trailRenderer->GetTileScale();
					if (ImGui::DragFloat("繰り返しの倍率", &tileScale, 0.1f, 0.1f, 100.0f))
					{
						trailRenderer->SetTileScale(tileScale);
					}
				}

				ImGui::Separator();
				ImGui::Text("フェードの設定:");

				// 幅フェード
				bool widthFade = trailRenderer->GetWidthFade();
				if (ImGui::Checkbox("太さを細くしていく", &widthFade))
				{
					trailRenderer->SetWidthFade(widthFade);
				}
				ImGui::SetItemTooltip("終わりに向かって細くなる");

				// アルファフェード
				bool alphaFade = trailRenderer->GetAlphaFade();
				if (ImGui::Checkbox("アルファを薄くしていく", &alphaFade))
				{
					trailRenderer->SetAlphaFade(alphaFade);
				}
				ImGui::SetItemTooltip("終わりに向かって透明になる");

				// ビルボード設定
				bool billboard = trailRenderer->GetBillboard();
				if (ImGui::Checkbox("ビルボード", &billboard))
				{
					trailRenderer->SetBillboard(billboard);
				}
				ImGui::SetItemTooltip("トレイルの各区間をカメラに向ける");

				// TextureManagerから読み込み済みテクスチャを取得
				auto texturePaths = TextureManager::GetInstance()->GetLoadedTexturePaths();

				if (!texturePaths.empty())
				{
					ImGui::Separator();
					ImGui::Text("テクスチャ:");

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
							if (displayName.empty()) displayName = "不明";

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

				// ティントカラー
				Vector4 tintColor = trailRenderer->GetTintColor();
				if (ImGui::ColorEdit4("色味##Trail", &tintColor.x))
				{
					trailRenderer->SetTintColor(tintColor);
				}
			}
			// Mesh Renderer特有の設定
			else if (auto* meshRenderer = dynamic_cast<MeshRenderer*>(renderer))
			{
				ImGui::Separator();
				ImGui::Text("メッシュの設定:");

				const char* primitiveTypes[] = { "Plane", "Ring", "Cylinder", "Sphere", "Torus", "Star", "Heart", "Spiral", "Cone", "Cube" };
				int primType = static_cast<int>(meshRenderer->GetPrimitiveType());
				PrimitiveOptions options = meshRenderer->GetOptions();
				bool optionsChanged = false;

				if (ImGui::Combo("形", &primType, primitiveTypes, 10))
				{
					meshRenderer->SetPrimitive(static_cast<PrimitiveType>(primType), options);
				}

				// プリミティブタイプに応じたオプションを表示
				PrimitiveType currentType = static_cast<PrimitiveType>(primType);
				
				// Segments (共通)
				if (currentType != PrimitiveType::Plane && currentType != PrimitiveType::Cube)
				{
					int segments = static_cast<int>(options.segments);
					if (ImGui::SliderInt("分割数", &segments, 4, 64))
					{
						options.segments = static_cast<uint32_t>(segments);
						optionsChanged = true;
					}
				}

				// Rings (Sphere)
				if (currentType == PrimitiveType::Sphere)
				{
					int rings = static_cast<int>(options.rings);
					if (ImGui::SliderInt("輪の数", &rings, 4, 32))
					{
						options.rings = static_cast<uint32_t>(rings);
						optionsChanged = true;
					}
				}

				// Inner/Outer Radius (Ring, Star, Torus)
				if (currentType == PrimitiveType::Ring || currentType == PrimitiveType::Star)
				{
					if (ImGui::DragFloat("内側の半径", &options.innerRadius, 0.01f, 0.0f, 1.0f))
					{
						optionsChanged = true;
					}
					if (ImGui::DragFloat("外側の半径", &options.outerRadius, 0.01f, 0.1f, 2.0f))
					{
						optionsChanged = true;
					}
				}

				// Tube Radius (Torus)
				if (currentType == PrimitiveType::Torus)
				{
					if (ImGui::DragFloat("管の半径", &options.tubeRadius, 0.01f, 0.05f, 0.5f))
					{
						optionsChanged = true;
					}
				}

				// Points (Star)
				if (currentType == PrimitiveType::Star)
				{
					int points = static_cast<int>(options.points);
					if (ImGui::SliderInt("先の数", &points, 3, 12))
					{
						options.points = static_cast<uint32_t>(points);
						optionsChanged = true;
					}
				}

				// Turns (Spiral)
				if (currentType == PrimitiveType::Spiral)
				{
					if (ImGui::DragFloat("巻き数", &options.turns, 0.1f, 0.5f, 10.0f))
					{
						optionsChanged = true;
					}
				}

				// Caps (Cylinder, Cone)
				if (currentType == PrimitiveType::Cylinder || currentType == PrimitiveType::Cone)
				{
					if (ImGui::Checkbox("ふたを付ける", &options.withCaps))
					{
						optionsChanged = true;
					}
				}

				// Double Sided (Plane)
				if (currentType == PrimitiveType::Plane)
				{
					if (ImGui::Checkbox("両面", &options.doubleSided))
					{
						optionsChanged = true;
					}
				}

				// Cube Options
				if (currentType == PrimitiveType::Cube)
				{
					if (ImGui::DragFloat3("大きさ (XYZ)", &options.cubeSize.x, 0.01f, 0.0f, 100.0f))
					{
						optionsChanged = true;
					}

					ImGui::Text("見せる面:");
					// 2列で表示
					if (ImGui::Checkbox("前##Cube", &options.cubeFaceVisible[0])) optionsChanged = true;
					ImGui::SameLine();
					if (ImGui::Checkbox("後ろ##Cube", &options.cubeFaceVisible[1])) optionsChanged = true;

					if (ImGui::Checkbox("上##Cube", &options.cubeFaceVisible[2])) optionsChanged = true;
					ImGui::SameLine();
					if (ImGui::Checkbox("下##Cube", &options.cubeFaceVisible[3])) optionsChanged = true;

					if (ImGui::Checkbox("右##Cube", &options.cubeFaceVisible[4])) optionsChanged = true;
					ImGui::SameLine();
					if (ImGui::Checkbox("左##Cube", &options.cubeFaceVisible[5])) optionsChanged = true;
				}

				// オプション変更適用
				if (optionsChanged)
				{
					meshRenderer->SetPrimitive(currentType, options);
				}

				float scale = meshRenderer->GetScale();
				if (ImGui::DragFloat("スケール", &scale, 0.01f, 0.01f, 10.0f))
				{
					meshRenderer->SetScale(scale);
				}

				// ビルボード設定
				bool useBillboard = meshRenderer->GetBillboard();
				if (ImGui::Checkbox("ビルボード", &useBillboard))
				{
					meshRenderer->SetBillboard(useBillboard);
				}

				// ティントカラー
				Vector4 tintColor = meshRenderer->GetTintColor();
				if (ImGui::ColorEdit4("色味", &tintColor.x))
				{
					meshRenderer->SetTintColor(tintColor);
				}

				// TextureManagerから読み込み済みテクスチャを取得
				auto texturePaths = TextureManager::GetInstance()->GetLoadedTexturePaths();

				if (!texturePaths.empty())
				{
					ImGui::Text("テクスチャ:");

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
							if (displayName.empty()) displayName = "不明";

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
			ImGui::Text("レンダラーがない");

			if (ImGui::Button("スプライトのレンダラーを作る"))
			{
				auto newRenderer = std::make_unique<SpriteRenderer>();
				newRenderer->Initialize("./Resources/uvChecker.png");
				emitter->SetRenderer(std::move(newRenderer));
			}
		}
		
	}
}

void ParticleEditor::DrawCurveEditor() { /* TODO: Advanced curve editing */ }
void ParticleEditor::DrawGradientEditor() { /* TODO: Gradient color editing */ }

void ParticleEditor::AddEmitterDialog()
{
	ImGui::OpenPopup("エミッターを追加###AddEmitter");

	if (ImGui::BeginPopupModal("エミッターを追加###AddEmitter", &showAddEmitterDialog_, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputText("エミッター名", emitterNameBuffer_, sizeof(emitterNameBuffer_));

		if (ImGui::Button("作成"))
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
		if (ImGui::Button("キャンセル")) { showAddEmitterDialog_ = false; }

		ImGui::EndPopup();
	}
}

void ParticleEditor::AddModuleDialog(ParticleEmitter* emitter)
{
	if (!emitter) return;

	ImGui::OpenPopup("モジュールを追加###AddModule");

	if (ImGui::BeginPopupModal("モジュールを追加###AddModule", &showAddModuleDialog_, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static int selectedCategory = 0; // 0=Spawn, 1=Update
		ImGui::RadioButton("発生", &selectedCategory, 0);
		ImGui::SameLine();
		ImGui::RadioButton("更新", &selectedCategory, 1);
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
				"Initial Rotation",
				"Radial Velocity",
				"Assign Ribbon ID"
			};
			const char* spawnDescriptions[] = {
				"一定の割合でパーティクルを出す",
				"間隔を空けてまとめて出す",
				"形（Box、Sphere、Cone など）から出す",
				"出たときの寿命を決める",
				"出たときの速度と向きを決める",
				"出たときの大きさを決める",
				"出たときの色 (RGBA) を決める",
				"出たときの回転角を決める",
				"エミッターの中心から放射状に飛ばす（爆発）",
				"複数トレイル用に RibbonId を振る（Niagara 風の分け方）"
			};
			
			ImGui::SetNextItemWidth(200.0f);
			ImGui::Combo("発生モジュール", &selectedModule, spawnModules, IM_ARRAYSIZE(spawnModules));
			
			// 説明表示
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", 
				(selectedModule >= 0 && selectedModule < IM_ARRAYSIZE(spawnDescriptions)) 
				? spawnDescriptions[selectedModule] : "");

			if (ImGui::Button("追加"))
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
				case 8: emitter->AddModule(std::make_unique<RadialVelocityModule>()); break;
				case 9: emitter->AddModule(std::make_unique<AssignRibbonIdModule>()); break;
				}
				showAddModuleDialog_ = false;
			}
		}
		else
		{
			// Update モジュール
			bool isGPUMode = (emitter->GetSimulationMode() == SimulationMode::GPU);

			// GPUモード時はGPU対応モジュールのみ表示するために動的フィルタリング
			std::vector<const char*> updateModules;
			std::vector<const char*> updateDescriptions;
			std::vector<int> originalIndices;

			auto AddModuleOption = [&](const char* name, const char* desc, int origIndex, bool gpuSupported)
			{
				if (!isGPUMode || gpuSupported)
				{
					updateModules.push_back(name);
					updateDescriptions.push_back(desc);
					originalIndices.push_back(origIndex);
				}
			};

			AddModuleOption("Gravity", "重力をかける（Y の下向き）", 0, true);
			AddModuleOption("Drag", "空気抵抗で遅くする", 1, true);
			AddModuleOption("Color Fade", "寿命に合わせて色を変える", 2, true);
			AddModuleOption("Scale Over Lifetime", "寿命に合わせて大きさを変える", 3, true);
			AddModuleOption("Rotation Over Lifetime", "寿命に合わせて回す", 4, true);
			AddModuleOption("Texture Sheet", "スプライトシートの UV アニメーション", 5, false);
			AddModuleOption("Attractor", "1点に引き寄せる・遠ざける", 6, true);
			AddModuleOption("Vortex", "渦を巻かせる", 7, true);
			AddModuleOption("Orbit", "中心の周りを回る", 8, false);
			AddModuleOption("Noise", "ランダムに揺らす", 9, true);
			AddModuleOption("Velocity Limit", "最大速度を制限する", 10, false);
			AddModuleOption("Acceleration", "一定の加速度をかける", 11, false);
			AddModuleOption("Curl Noise", "3D のカールノイズで乱れを加える", 12, true);
			AddModuleOption("Size By Speed", "速さに合わせて大きさを変える", 13, false);
			AddModuleOption("Color By Speed", "速さに合わせて色を変える", 14, false);
			AddModuleOption("Collision", "平面や箱に当たって跳ね返る", 15, false);
			AddModuleOption("Kill Zone", "範囲の内側・外側で消す", 16, false);
			AddModuleOption("Sprint To Target", "目標の位置へ加速していく", 17, false);
			AddModuleOption("Sub Emitter", "パーティクルの出来事でサブエフェクトを出す", 18, false);
			AddModuleOption("Velocity Over Lifetime", "寿命に合わせて速度に掛ける", 19, true);
			AddModuleOption("Stretch By Velocity", "進む方向に伸ばす（弾、雨）", 20, true);
			AddModuleOption("Wind", "乱れのある風を当てる", 21, false);
			AddModuleOption("Flicker", "アルファをちらつかせる（炎、火花）", 22, true);
			AddModuleOption("Alpha Fade", "寿命に合わせてアルファを薄くする", 23, true);
			AddModuleOption("Rotation By Speed", "動く速さに合わせて回す", 24, false);
			AddModuleOption("Sine Wave", "サイン波で位置を揺らす", 25, false);
			AddModuleOption("Spiral", "らせんを描いて動く", 26, false);
			AddModuleOption("Twist", "軸の周りにねじる", 27, false);
			AddModuleOption("Face Velocity", "パーティクルの向きを進む方向に合わせる", 28, true);
			AddModuleOption("Jitter", "毎フレーム位置をランダムにずらす", 29, false);
			AddModuleOption("Force Over Lifetime", "寿命に合わせて変わる力をかける", 30, false);

			// クランプ処理（切り替え時にインデックスがはみ出ないようにする）
			if (selectedModule >= static_cast<int>(updateModules.size()))
			{
				selectedModule = 0;
			}

			ImGui::SetNextItemWidth(200.0f);
			ImGui::Combo("更新モジュール", &selectedModule, updateModules.data(), static_cast<int>(updateModules.size()));
			
			// 説明表示
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s",
				(selectedModule >= 0 && selectedModule < static_cast<int>(updateDescriptions.size()))
				? updateDescriptions[selectedModule] : "");

			if (ImGui::Button("追加"))
			{
				int actualIndex = originalIndices[selectedModule];
				switch (actualIndex)
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
				// Motion effect modules
				case 19: emitter->AddModule(std::make_unique<VelocityOverLifetimeModule>()); break;
				case 20: emitter->AddModule(std::make_unique<StretchByVelocityModule>()); break;
				case 21: emitter->AddModule(std::make_unique<WindModule>()); break;
				case 22: emitter->AddModule(std::make_unique<FlickerModule>()); break;
				case 23: emitter->AddModule(std::make_unique<AlphaFadeModule>()); break;
				case 24: emitter->AddModule(std::make_unique<RotationBySpeedModule>()); break;
				case 25: emitter->AddModule(std::make_unique<SineWaveModule>()); break;
				case 26: emitter->AddModule(std::make_unique<SpiralModule>()); break;
				case 27: emitter->AddModule(std::make_unique<TwistModule>()); break;
				case 28: emitter->AddModule(std::make_unique<FaceVelocityModule>()); break;
				case 29: emitter->AddModule(std::make_unique<JitterModule>()); break;
				case 30: emitter->AddModule(std::make_unique<ForceOverLifetimeModule>()); break;
				}
				showAddModuleDialog_ = false;
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("キャンセル")) { showAddModuleDialog_ = false; }

		ImGui::EndPopup();
	}
}
#endif
} // namespace KCE
