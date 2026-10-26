#include "ParticleEditor.h"
#include "effects/particle/ParticleEffect.h"
#include "effects/particle/ParticleEmitter.h"
#include "effects/particle/ParticleManager.h"
#include "base/PathManager.h"
#include "effects/particle/diagnostics/ParticleDiagnostics.h"
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
#include "manager/effect/PostProcessManager.h"
#include "math/BlendMode.h"
#include "time/TimeManager.h"
#include "time/Timer.h"
#include <filesystem>
#include <algorithm>
#include <type_traits>

#include "effects/particle/serialization/ParticleEffectSerializer.h"
#include "effects/particle/ParticleEasing.h"
#include <cctype>
#include <cfloat>
#include <unordered_map>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
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
	
	// レイアウト: この幅以上ならアウトライナーとインスペクタを横並びにする
	constexpr float kWideLayoutWidth = 720.0f;
	constexpr float kSplitterHeight = 6.0f;
	constexpr float kMinOutlinerHeight = 80.0f;
	constexpr float kEmitterRowExtraPadding = 4.0f; // アウトライナーのエミッター行に足す縦余白

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

void ParticleEditor::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, PostProcessManager* postProcessManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	postProcessManager_ = postProcessManager;

	// 初期エフェクトを作成
	NewEffect();

#ifdef USE_IMGUI
	DebugUIManager::GetInstance()->RegisterWindow(this, "パーティクルエディター###Particle Editor", [this]() { this->DrawImGui(); }, EditorDock::Right);
#endif
}

void ParticleEditor::DrawImGui()
{
#ifdef USE_IMGUI
	// 選択状態の検証（削除・読み込みで無効になった選択を解除する）
	auto* selectedEmitter = GetSelectedEmitter();
	if (!selectedEmitter)
	{
		selectedEmitterIndex_ = -1;
		selectedModule_ = nullptr;
		showAddModuleDialog_ = false;
		if (inspectorTarget_ != InspectorTarget::Effect) inspectorTarget_ = InspectorTarget::Effect;
	}
	else if (inspectorTarget_ == InspectorTarget::Module && !GetSelectedModule(selectedEmitter))
	{
		selectedModule_ = nullptr;
		inspectorTarget_ = InspectorTarget::Emitter;
	}

	ImGui::PushID(this);
	ImGui::PushID("Toolbar"); DrawToolbar(); ImGui::PopID();
	ImGui::Separator();

	// 幅が十分あれば左右2ペイン、狭ければ上下（スプリッタで高さ調整）
	const ImVec2 available = ImGui::GetContentRegionAvail();
	if (available.x >= kWideLayoutWidth)
	{
		if (ImGui::BeginTable("##ParticleEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
		{
			ImGui::TableSetupColumn("Outliner", ImGuiTableColumnFlags_WidthStretch, 0.4f);
			ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 0.6f);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::BeginChild("##Outliner", ImVec2(0.0f, available.y));
			DrawOutliner();
			ImGui::EndChild();
			ImGui::TableNextColumn();
			ImGui::BeginChild("##Inspector", ImVec2(0.0f, available.y));
			DrawInspector();
			ImGui::EndChild();
			ImGui::EndTable();
		}
	}
	else
	{
		const float maxOutlinerHeight = (std::max)(kMinOutlinerHeight, available.y - kMinOutlinerHeight);
		outlinerHeight_ = (std::clamp)(outlinerHeight_, kMinOutlinerHeight, maxOutlinerHeight);
		ImGui::BeginChild("##Outliner", ImVec2(0.0f, outlinerHeight_), ImGuiChildFlags_Borders);
		DrawOutliner();
		ImGui::EndChild();

		// スプリッタ
		ImGui::InvisibleButton("##OutlinerSplitter", ImVec2(-1.0f, kSplitterHeight));
		if (ImGui::IsItemActive()) outlinerHeight_ += ImGui::GetIO().MouseDelta.y;
		if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		const ImVec2 splitterMin = ImGui::GetItemRectMin();
		const ImVec2 splitterMax = ImGui::GetItemRectMax();
		const float splitterY = (splitterMin.y + splitterMax.y) * 0.5f;
		ImGui::GetWindowDrawList()->AddLine(ImVec2(splitterMin.x, splitterY), ImVec2(splitterMax.x, splitterY),
			ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_SeparatorActive : ImGuiCol_Separator), 2.0f);

		ImGui::BeginChild("##Inspector", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
		DrawInspector();
		ImGui::EndChild();
	}

	// ダイアログ
	if (showAddEmitterDialog_) AddEmitterDialog();
	if (showAddModuleDialog_)
	{
		if (auto* emitter = GetSelectedEmitter()) AddModuleDialog(emitter);
	}
	if (showSaveAsDialog_) SaveAsDialog();
	DiscardConfirmDialog();

	// エミッターのリネーム
	if (openRenamePopup_)
	{
		ImGui::OpenPopup("Rename Emitter");
		openRenamePopup_ = false;
	}
	if (ImGui::BeginPopupModal("Rename Emitter", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(220.0f);
		const bool submitted = ImGui::InputText("##RenameEmitter", renameBuffer_, sizeof(renameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);
		if ((ImGui::Button("OK") || submitted) && renameBuffer_[0] != '\0')
		{
			if (currentEffect_ && renameEmitterIndex_ >= 0)
			{
				if (auto* emitter = currentEffect_->GetEmitter(static_cast<size_t>(renameEmitterIndex_)))
				{
					emitter->SetName(renameBuffer_);
					MarkDirty();
				}
			}
			renameEmitterIndex_ = -1;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { renameEmitterIndex_ = -1; ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
	ImGui::PopID();
#endif
}

void ParticleEditor::Update(CameraManager* camera)
{
	(void)camera;
	float dt = TimeManager::GetInstance().GetGameContext().deltaTime;

	// ループプレビュー：一定間隔で全エミッターを再スタート（一時停止中は進めず、再生速度に合わせる）
	if (previewLooping_ && currentEffect_ && !currentEffect_->IsPaused())
	{
		previewElapsed_ += dt * currentEffect_->GetPlaybackSpeed();
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
		// エディタは同一エフェクトを使い回さないため、プールに退避したまま
		// GPUディスクリプタ/バッファを保持し続けないよう即座に解放する。
		ParticleManager::GetInstance()->PurgeEffectPools();
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
	selectedModule_ = nullptr;
	inspectorTarget_ = InspectorTarget::Effect;
	previewSourceId_ = 0;
	effectPath_.clear();
	strcpy_s(effectNameBuffer_, "NewEffect");
	isDirty_ = false;
}

void ParticleEditor::CloseCurrentEffect()
{
	currentEffect_ = nullptr;
	selectedEmitterIndex_ = -1;
	selectedModule_ = nullptr;
	inspectorTarget_ = InspectorTarget::Effect;
	previewSourceId_ = 0;
	effectPath_.clear();
	strcpy_s(effectNameBuffer_, "");
	isDirty_ = false;
}

bool ParticleEditor::SelectEmitter(size_t index)
{
	if (!currentEffect_ || index >= currentEffect_->GetEmitterCount() || !currentEffect_->GetEmitter(index))
	{
		selectedEmitterIndex_ = -1;
		selectedModule_ = nullptr;
		inspectorTarget_ = InspectorTarget::Effect;
		return false;
	}
	selectedEmitterIndex_ = static_cast<int>(index);
	selectedModule_ = nullptr;
	inspectorTarget_ = InspectorTarget::Emitter;
	return true;
}

bool ParticleEditor::LoadEffect(const std::string& path)
{
	// ファイルからエフェクトを読み込み
	auto effect = ParticleEffect::LoadFromFile(path);
	if (effect)
	{
		// 既存のエフェクトを削除
		if (currentEffect_)
		{
			ParticleManager::GetInstance()->RemoveEffect(currentEffect_);
			// エディタは同一エフェクトを使い回さないため、プールに退避したまま
			// GPUディスクリプタ/バッファを保持し続けないよう即座に解放する。
			ParticleManager::GetInstance()->PurgeEffectPools();
		}

		// エディタ用なので自動削除しない
		effect->SetAutoRemove(false);
		effectPath_ = path;
		strcpy_s(effectNameBuffer_, effect->GetName().c_str());
		effect->Play();

		// マネージャーに登録してポインタを保持
		currentEffect_ = effect.get();
		ParticleManager::GetInstance()->AddEffect(std::move(effect));
		selectedEmitterIndex_ = -1;
		selectedModule_ = nullptr;
		inspectorTarget_ = InspectorTarget::Effect;
		previewSourceId_ = 0;
		isDirty_ = false;
		operationSucceeded_ = true;
		operationStatus_ = "Loaded: " + path;
		return true;
	}
	operationSucceeded_ = false;
	operationStatus_ = "Load failed: " + path + " (invalid JSON, unsupported value, missing file, or resource initialization failure)";
	return false;
}

bool ParticleEditor::SaveEffect(const std::string& path)
{
	if (!currentEffect_)
	{
		operationSucceeded_ = false;
		operationStatus_ = "Save failed: no effect is open";
		return false;
	}
	try
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
			// Particle JSON is stored below Resources/json/particles.
			std::filesystem::path dir = PathManager::ResolveApplicationResource("json/particles");
			std::error_code directoryError;
			std::filesystem::create_directories(dir, directoryError);
			if (directoryError)
			{
				operationSucceeded_ = false;
				operationStatus_ = "Save failed: cannot create directory " + dir.string();
				return false;
			}
			savePath = (dir / (currentEffect_->GetName() + ".json")).string();
		}
		
		// ファイルに保存
		if (!currentEffect_->SaveToFile(savePath))
		{
			operationSucceeded_ = false;
			operationStatus_ = "Save failed: " + savePath;
			return false;
		}
		effectPath_ = savePath;
		isDirty_ = false;
		operationSucceeded_ = true;
		operationStatus_ = "Saved: " + savePath;
		return true;
	}
	catch (const std::exception& error)
	{
		operationSucceeded_ = false;
		operationStatus_ = std::string("Save failed: ") + error.what();
		return false;
	}
}

#ifdef USE_IMGUI
namespace
{
	constexpr float kPreviewStepSeconds = 1.0f / 60.0f;

	const ImVec4 kSpawnColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
	const ImVec4 kUpdateColor = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
	const ImVec4 kRendererColor = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
	const ImVec4 kWarningColor = ImVec4(1.0f, 0.65f, 0.2f, 1.0f);
	const ImVec4 kDirtyColor = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);

	struct ParticleJsonEntry
	{
		std::filesystem::path path;
		std::string label;
	};

	/** パーティクルJSONの保存先（新規保存は常にここ） */
	std::filesystem::path GetParticleDirectory()
	{
		return PathManager::ResolveApplicationResource("json/particles");
	}

	/** 読み込み可能なパーティクルJSONを列挙（旧パスも含む） */
	std::vector<ParticleJsonEntry> CollectParticleFiles()
	{
		std::vector<ParticleJsonEntry> particleFiles;
		const std::filesystem::path directories[] = {
			GetParticleDirectory(),
			PathManager::ResolveApplicationResource("json/particle"), // legacy singular path
			PathManager::ResolveApplicationResource("particles")     // legacy root path
		};
		for (const auto& dir : directories)
		{
			std::error_code iteratorError;
			if (!std::filesystem::is_directory(dir, iteratorError)) continue;
			for (std::filesystem::recursive_directory_iterator it(
				dir, std::filesystem::directory_options::skip_permission_denied, iteratorError), end;
				it != end; it.increment(iteratorError))
			{
				if (iteratorError) { iteratorError.clear(); continue; }
				if (!it->is_regular_file(iteratorError) || it->path().extension() != ".json") continue;
				const auto relative = std::filesystem::relative(it->path(), dir, iteratorError);
				particleFiles.push_back({ it->path(), iteratorError ? it->path().filename().generic_string() : relative.generic_string() });
				iteratorError.clear();
			}
		}
		std::sort(particleFiles.begin(), particleFiles.end(), [](const ParticleJsonEntry& a, const ParticleJsonEntry& b)
		{
			return a.label < b.label;
		});
		particleFiles.erase(std::unique(particleFiles.begin(), particleFiles.end(), [](const ParticleJsonEntry& a, const ParticleJsonEntry& b)
		{
			std::error_code error;
			return std::filesystem::equivalent(a.path, b.path, error) && !error;
		}), particleFiles.end());
		return particleFiles;
	}

	bool ContainsIgnoreCase(const std::string& text, const char* filter)
	{
		if (!filter || filter[0] == '\0') return true;
		std::string lowerText = text;
		std::string lowerFilter = filter;
		auto toLower = [](std::string& value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		};
		toLower(lowerText);
		toLower(lowerFilter);
		return lowerText.find(lowerFilter) != std::string::npos;
	}

	std::string FileNameOf(const std::string& path)
	{
		const size_t lastSlash = path.find_last_of("/\\");
		std::string name = lastSlash == std::string::npos ? path : path.substr(lastSlash + 1);
		return name.empty() ? std::string("(None)") : name;
	}

	//===== モジュールのカテゴリと説明 =====//

	struct ModuleInfo
	{
		const char* category;
		const char* description;
	};

	constexpr const char* kModuleCategories[] = {
		"Spawn", "Initial", "Force", "Motion", "Color / Alpha", "Size / Rotation", "Collision / Lifetime", "Ribbon / Texture", "Other"
	};

	ModuleInfo GetModuleInfo(const std::string& id)
	{
		static const std::unordered_map<std::string, ModuleInfo> table = {
			{ "SpawnRate",            { "Spawn", "一定レートでパーティクルを生成する" } },
			{ "SpawnBurst",           { "Spawn", "指定数を一度に生成する（間隔・ループ指定可）" } },
			{ "SpawnShape",           { "Spawn", "球・円・箱・コーン・線などの形状から生成する" } },
			{ "InitialPosition",      { "Initial", "生成位置にランダムなオフセットを加える" } },
			{ "InitialVelocity",      { "Initial", "初速度を範囲指定でランダムに設定する" } },
			{ "InitialLifetime",      { "Initial", "寿命を範囲指定でランダムに設定する" } },
			{ "InitialColor",         { "Initial", "初期カラー（RGBA）を範囲指定で設定する" } },
			{ "InitialScale",         { "Initial", "初期サイズを範囲指定で設定する" } },
			{ "InitialRotation",      { "Initial", "初期回転角を範囲指定で設定する" } },
			{ "RadialVelocity",       { "Initial", "エミッター中心から放射状の速度を与える（爆発など）" } },
			{ "AssignRibbonId",       { "Ribbon / Texture", "複数トレイル用にRibbonIdを割り当てる" } },
			{ "RibbonInterpolation",  { "Ribbon / Texture", "リボンの間隔が開きすぎないよう補間点を生成する" } },
			{ "MultiSourceRibbon",    { "Ribbon / Texture", "登録した複数ソースからリボンを生成する" } },
			{ "TextureSheet",         { "Ribbon / Texture", "スプライトシートのUVアニメーション" } },
			{ "Gravity",              { "Force", "重力を加える" } },
			{ "Drag",                 { "Force", "空気抵抗で減速させる" } },
			{ "Acceleration",         { "Force", "一定の加速度を加える" } },
			{ "Attractor",            { "Force", "指定点へ引き寄せる／反発させる" } },
			{ "Vortex",               { "Force", "軸を中心に渦を巻かせる" } },
			{ "Wind",                 { "Force", "乱流つきの風を加える" } },
			{ "Noise",                { "Force", "ランダムなノイズ移動を加える" } },
			{ "CurlNoise",            { "Force", "3Dカールノイズで乱流を加える" } },
			{ "ForceOverLifetime",    { "Force", "寿命に応じて変化する方向力を加える" } },
			{ "SprintToTarget",       { "Force", "ターゲット位置へ加速して向かう" } },
			{ "Jitter",               { "Motion", "毎フレーム位置をランダムに揺らす" } },
			{ "Orbit",                { "Motion", "中心点の周りを公転させる" } },
			{ "SineWave",             { "Motion", "サイン波で位置を揺らす" } },
			{ "Spiral",               { "Motion", "らせん状に移動させる" } },
			{ "Twist",                { "Motion", "軸周りにねじる" } },
			{ "VelocityOverLifetime", { "Motion", "寿命に応じて速度を倍率変化させる" } },
			{ "VelocityLimit",        { "Motion", "最大速度を制限する" } },
			{ "ColorFade",            { "Color / Alpha", "寿命に応じて色を変化させる（グラデーション可）" } },
			{ "AlphaFade",            { "Color / Alpha", "寿命に応じてアルファをフェードする" } },
			{ "Flicker",              { "Color / Alpha", "アルファを明滅させる（炎・火花など）" } },
			{ "ColorBySpeed",         { "Color / Alpha", "速度に応じて色を変える" } },
			{ "ScaleOverLifetime",    { "Size / Rotation", "寿命に応じてサイズを変化させる" } },
			{ "SizeBySpeed",          { "Size / Rotation", "速度に応じてサイズを変える" } },
			{ "StretchByVelocity",    { "Size / Rotation", "進行方向に引き伸ばす（弾・雨など）" } },
			{ "RotationOverLifetime", { "Size / Rotation", "寿命に応じて回転させる" } },
			{ "RotationBySpeed",      { "Size / Rotation", "移動速度に応じて回転させる" } },
			{ "FaceVelocity",         { "Size / Rotation", "回転を進行方向に合わせる" } },
			{ "Collision",            { "Collision / Lifetime", "平面・箱と衝突して跳ね返る" } },
			{ "KillZone",             { "Collision / Lifetime", "ゾーンの内側／外側のパーティクルを消す" } },
			{ "SubEmitter",           { "Collision / Lifetime", "生成・消滅などのイベントでサブエフェクトを出す" } },
		};
		auto it = table.find(id);
		return it == table.end() ? ModuleInfo{ "Other", "" } : it->second;
	}

	//===== プレビュー描画ヘルパー =====//

	/** 0..1の評価関数から色のランプ（グラデーションバー）を描画する。アルファは市松模様で表示 */
	template<class Evaluate>
	void DrawColorRamp(Evaluate evaluate, float height = 16.0f)
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const float width = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
		const ImVec2 end = ImVec2(origin.x + width, origin.y + height);

		// 透明度が分かるよう市松模様の背景を敷く
		const float cell = height * 0.5f;
		for (float x = 0.0f; x < width; x += cell)
		{
			for (int row = 0; row < 2; ++row)
			{
				const bool dark = (static_cast<int>(x / cell) + row) % 2 == 0;
				drawList->AddRectFilled(ImVec2(origin.x + x, origin.y + row * cell),
					ImVec2((std::min)(origin.x + x + cell, end.x), origin.y + (row + 1) * cell),
					dark ? IM_COL32(90, 90, 90, 255) : IM_COL32(150, 150, 150, 255));
			}
		}

		constexpr int kSteps = 48;
		for (int i = 0; i < kSteps; ++i)
		{
			const float t0 = static_cast<float>(i) / kSteps;
			const float t1 = static_cast<float>(i + 1) / kSteps;
			const Vector4 c0 = evaluate(t0);
			const Vector4 c1 = evaluate(t1);
			const ImU32 left = ImGui::ColorConvertFloat4ToU32(ImVec4(c0.x, c0.y, c0.z, c0.w));
			const ImU32 right = ImGui::ColorConvertFloat4ToU32(ImVec4(c1.x, c1.y, c1.z, c1.w));
			drawList->AddRectFilledMultiColor(ImVec2(origin.x + width * t0, origin.y), ImVec2(origin.x + width * t1, end.y),
				left, right, right, left);
		}
		drawList->AddRect(origin, end, IM_COL32(20, 20, 20, 255));
		ImGui::Dummy(ImVec2(width, height));
	}

	/** 0..1の評価関数からカーブをプロットする */
	template<class Evaluate>
	void DrawCurvePlot(const char* id, Evaluate evaluate, float height = 50.0f)
	{
		constexpr int kSamples = 64;
		float values[kSamples];
		float minimum = FLT_MAX;
		float maximum = -FLT_MAX;
		for (int i = 0; i < kSamples; ++i)
		{
			values[i] = evaluate(static_cast<float>(i) / (kSamples - 1));
			minimum = (std::min)(minimum, values[i]);
			maximum = (std::max)(maximum, values[i]);
		}
		if (maximum - minimum < 1e-4f) { minimum -= 0.5f; maximum += 0.5f; }
		const float padding = (maximum - minimum) * 0.1f;
		ImGui::PlotLines(id, values, kSamples, 0, nullptr, minimum - padding, maximum + padding, ImVec2(-1.0f, height));
	}

	void DrawTextureThumbnail(const std::string& path, float size)
	{
		uint32_t index = 0;
		auto* textureManager = TextureManager::GetInstance();
		if (path.empty() || !textureManager->TryGetTextureIndexByFilePath(path, index))
		{
			ImGui::Dummy(ImVec2(size, size));
			return;
		}
		ImGui::Image(static_cast<ImTextureID>(textureManager->GetSrvHandleGPU(index).ptr), ImVec2(size, size));
	}

	/**
	 * @brief 読み込み済みテクスチャから選択するコンボ（サムネイル付き）
	 * @return 選択が変わったらtrue（outPathに新しいパス）
	 */
	bool DrawTextureCombo(const char* id, const std::string& currentPath, std::string& outPath)
	{
		auto texturePaths = TextureManager::GetInstance()->GetLoadedTexturePaths();
		bool changed = false;

		DrawTextureThumbnail(currentPath, ImGui::GetFrameHeight() * 2.0f);
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::TextUnformatted("Texture");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo(id, FileNameOf(currentPath).c_str(), ImGuiComboFlags_HeightLarge))
		{
			const float thumbnail = ImGui::GetTextLineHeight() * 1.5f;
			for (int i = 0; i < static_cast<int>(texturePaths.size()); ++i)
			{
				ImGui::PushID(i);
				const bool isSelected = texturePaths[i] == currentPath;
				DrawTextureThumbnail(texturePaths[i], thumbnail);
				ImGui::SameLine();
				if (ImGui::Selectable(FileNameOf(texturePaths[i]).c_str(), isSelected, 0, ImVec2(0.0f, thumbnail)))
				{
					outPath = texturePaths[i];
					changed = true;
				}
				ImGui::SetItemTooltip("%s", texturePaths[i].c_str());
				if (isSelected) ImGui::SetItemDefaultFocus();
				ImGui::PopID();
			}
			if (texturePaths.empty()) ImGui::TextDisabled("(No textures loaded)");
			ImGui::EndCombo();
		}
		ImGui::EndGroup();
		return changed;
	}
}

void ParticleEditor::DrawToolbar()
{
	// Ctrl+S で保存（未保存の新規エフェクトは名前を付けて保存）
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
	{
		if (effectPath_.empty()) showSaveAsDialog_ = true;
		else SaveEffect(effectPath_);
	}

	//===== ファイル操作 =====//
	if (ImGui::Button("New")) { RunWithDiscardCheck([this]() { NewEffect(); }); }
	ImGui::SameLine();
	if (ImGui::Button("Open...")) { loadFilterBuffer_[0] = '\0'; ImGui::OpenPopup("OpenEffectPopup"); }
	if (ImGui::BeginPopup("OpenEffectPopup"))
	{
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(260.0f);
		ImGui::InputTextWithHint("##LoadFilter", "Search...", loadFilterBuffer_, sizeof(loadFilterBuffer_));
		ImGui::Separator();
		ImGui::BeginChild("##LoadList", ImVec2(260.0f, 300.0f));
		bool anyShown = false;
		for (const auto& entry : CollectParticleFiles())
		{
			if (!ContainsIgnoreCase(entry.label, loadFilterBuffer_)) continue;
			anyShown = true;
			if (ImGui::Selectable(entry.label.c_str(), entry.path.string() == effectPath_))
			{
				const std::string path = entry.path.string();
				ImGui::CloseCurrentPopup();
				RunWithDiscardCheck([this, path]() { LoadEffect(path); });
			}
		}
		if (!anyShown) ImGui::TextDisabled("(No particle files found)");
		ImGui::EndChild();
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, isDirty_ ? ImVec4(0.25f, 0.55f, 0.25f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
	if (ImGui::Button("Save"))
	{
		if (effectPath_.empty()) showSaveAsDialog_ = true;
		else SaveEffect(effectPath_);
	}
	ImGui::PopStyleColor();
	ImGui::SetItemTooltip("Ctrl+S");
	ImGui::SameLine();
	if (ImGui::Button("Save As...")) { showSaveAsDialog_ = true; }
	ImGui::SameLine();
	if (ImGui::Button("View")) ImGui::OpenPopup("ViewOptionsPopup");
	if (ImGui::BeginPopup("ViewOptionsPopup"))
	{
		ImGui::SeparatorText("Debug Draw");
		ImGui::Checkbox("Debug Lines (shapes / forces)", &showDebug_);
		ImGui::Checkbox("Particle Markers", &showParticleMarkers_);
		ImGui::SeparatorText("Scene");
		ImGui::SetNextItemWidth(180.0f);
		ImGui::ColorEdit3("Skydome Color", &skydomeColor_.x);
		ImGui::SeparatorText("Motion Dummy");
		ImGui::Checkbox("Enable Motion Dummy", &enablePreviewTarget_);
		ImGui::SetItemTooltip("トレイル確認用に、エフェクトを円軌道で動かすダミーを有効化します");
		if (enablePreviewTarget_)
		{
			ImGui::SetNextItemWidth(180.0f);
			ImGui::DragFloat("Motion Speed", &previewTargetSpeed_, 0.05f, 0.1f, 10.0f, "%.2f");
			ImGui::SetNextItemWidth(180.0f);
			ImGui::DragFloat("Motion Radius", &previewTargetRadius_, 0.1f, 0.5f, 20.0f, "%.1f");
		}
		ImGui::EndPopup();
	}

	//===== エフェクト名（未保存なら * 表示） =====//
	if (isDirty_)
	{
		ImGui::TextColored(kDirtyColor, "*");
		ImGui::SetItemTooltip("未保存の変更があります");
		ImGui::SameLine();
	}
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputTextWithHint("##EffectName", "Effect name", effectNameBuffer_, sizeof(effectNameBuffer_)))
	{
		if (currentEffect_) currentEffect_->SetName(effectNameBuffer_);
		MarkDirty();
	}

	if (!currentEffect_) return;

	//===== 再生制御 =====//
	const size_t emitterCount = currentEffect_->GetEmitterCount();
	const bool isPaused = currentEffect_->IsPaused();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
	if (ImGui::Button("Play"))
	{
		for (size_t i = 0; i < emitterCount; ++i)
		{
			if (auto* emitter = currentEffect_->GetEmitter(i)) emitter->Restart();
		}
		currentEffect_->SetPaused(false);
		currentEffect_->Play();
		previewElapsed_ = 0.0f;
	}
	ImGui::PopStyleColor();
	ImGui::SetItemTooltip("全エミッターを最初から再生（既存パーティクルは残す）");
	ImGui::SameLine();
	if (ImGui::Button(isPaused ? "Resume" : "Pause")) currentEffect_->SetPaused(!isPaused);
	ImGui::SameLine();
	ImGui::BeginDisabled(!isPaused);
	if (ImGui::Button("Step")) currentEffect_->StepFrame(kPreviewStepSeconds);
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("一時停止中に1フレーム(1/60秒)進める");
	ImGui::SameLine();
	if (ImGui::Button("Stop"))
	{
		currentEffect_->Stop();
		previewLooping_ = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		for (size_t i = 0; i < emitterCount; ++i)
		{
			if (auto* emitter = currentEffect_->GetEmitter(i)) emitter->Reset();
		}
		currentEffect_->SetPaused(false);
		currentEffect_->Play();
		previewElapsed_ = 0.0f;
	}
	ImGui::SetItemTooltip("パーティクルを全消去して再生し直す");

	float speed = currentEffect_->GetPlaybackSpeed();
	ImGui::SetNextItemWidth(140.0f);
	if (ImGui::SliderFloat("##PlaybackSpeed", &speed, 0.0f, 3.0f, "Speed %.2fx")) currentEffect_->SetPlaybackSpeed(speed);
	ImGui::SetItemTooltip("プレビュー再生速度（保存されません）");
	ImGui::SameLine();
	if (ImGui::SmallButton("1x")) currentEffect_->SetPlaybackSpeed(1.0f);
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &previewLooping_);
	ImGui::SetItemTooltip("一定間隔で自動的にPlayし直す");
	if (previewLooping_)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragFloat("##LoopInterval", &previewRepeatInterval_, 0.1f, 0.1f, 30.0f, "%.1f s");
		const float progress = previewRepeatInterval_ > 0.0f ? previewElapsed_ / previewRepeatInterval_ : 0.0f;
		ImGui::ProgressBar(progress, ImVec2(-1.0f, 4.0f), "");
	}

	//===== 統計・状態 =====//
	uint32_t totalParticles = 0;
	for (size_t i = 0; i < emitterCount; ++i)
	{
		if (const auto* emitter = currentEffect_->GetEmitter(i)) totalParticles += emitter->GetActiveParticleCount();
	}
	ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Particles: %u", totalParticles);
	ImGui::SameLine();
	ImGui::TextDisabled("| Emitters: %d | %s%s", static_cast<int>(emitterCount),
		currentEffect_->IsPlaying() ? "Playing" : "Stopped", isPaused ? " (Paused)" : "");
	if (!operationStatus_.empty())
	{
		const ImVec4 color = operationSucceeded_ ? ImVec4(0.35f, 0.9f, 0.45f, 1.0f) : ImVec4(1.0f, 0.35f, 0.3f, 1.0f);
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextColored(color, "%s", operationStatus_.c_str());
		ImGui::PopTextWrapPos();
	}
}

void ParticleEditor::DrawOutliner()
{
	if (!currentEffect_) return;

	// エフェクト全体の行
	const std::string effectLabel = std::string("Effect: ") + effectNameBuffer_ + "###EffectRow";
	if (ImGui::Selectable(effectLabel.c_str(), inspectorTarget_ == InspectorTarget::Effect))
	{
		inspectorTarget_ = InspectorTarget::Effect;
	}
	ImGui::Separator();

	enum class EmitterOp { None, Delete, Duplicate };
	EmitterOp pendingOp = EmitterOp::None;
	size_t pendingIndex = 0;

	for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
	{
		auto* emitter = currentEffect_->GetEmitter(i);
		if (!emitter) continue;
		ImGui::PushID(emitter);

		// エミッター行は選択・操作しやすいよう縦の余白を広げる
		const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePadding.x + 2.0f, framePadding.y + kEmitterRowExtraPadding));
		const float rowButtonSize = ImGui::GetFrameHeight();

		// ミュート / ソロ（プレビュー専用。保存されない）
		const bool muted = !emitter->IsEnabled();
		ImGui::PushStyleColor(ImGuiCol_Button, muted ? ImVec4(0.6f, 0.2f, 0.2f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Button));
		if (ImGui::Button("M", ImVec2(rowButtonSize, rowButtonSize))) SetEmitterMuted(emitter, !muted);
		ImGui::PopStyleColor();
		ImGui::SetItemTooltip(muted ? "Unmute" : "Mute（プレビュー専用）");
		ImGui::SameLine();
		if (ImGui::Button("S", ImVec2(rowButtonSize, rowButtonSize))) SoloEmitter(i);
		ImGui::SetItemTooltip("Solo（他のエミッターをミュート）");
		ImGui::SameLine();

		const bool isSelected = selectedEmitterIndex_ == static_cast<int>(i);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
		if (isSelected && inspectorTarget_ == InspectorTarget::Emitter) flags |= ImGuiTreeNodeFlags_Selected;
		if (isSelected) flags |= ImGuiTreeNodeFlags_DefaultOpen;

		if (muted) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		const bool open = ImGui::TreeNodeEx("##EmitterNode", flags, "%s", emitter->GetName().c_str());
		if (muted) ImGui::PopStyleColor();
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			selectedEmitterIndex_ = static_cast<int>(i);
			selectedModule_ = nullptr;
			inspectorTarget_ = InspectorTarget::Emitter;
		}
		if (ImGui::BeginPopupContextItem("EmitterContext"))
		{
			if (ImGui::MenuItem("Rename"))
			{
				renameEmitterIndex_ = static_cast<int>(i);
				strncpy_s(renameBuffer_, emitter->GetName().c_str(), _TRUNCATE);
				openRenamePopup_ = true;
			}
			if (ImGui::MenuItem("Duplicate")) { pendingOp = EmitterOp::Duplicate; pendingIndex = i; }
			if (ImGui::MenuItem("Restart"))
			{
				emitter->Restart();
				if (!currentEffect_->IsPlaying()) currentEffect_->Play();
			}
			if (ImGui::MenuItem("Stop")) emitter->Stop();
			ImGui::Separator();
			if (ImGui::MenuItem("Delete")) { pendingOp = EmitterOp::Delete; pendingIndex = i; }
			ImGui::EndPopup();
		}

		// 行の右端にパーティクル数
		char countText[32];
		snprintf(countText, sizeof(countText), "%u", emitter->GetActiveParticleCount());
		ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(countText).x);
		ImGui::TextColored(emitter->IsEmitting() ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", countText);
		ImGui::SetItemTooltip(emitter->IsEmitting() ? "Emitting / Active particles" : "Not emitting / Active particles");
		ImGui::PopStyleVar();

		if (open)
		{
			DrawModuleStack(emitter, i);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	if (currentEffect_->GetEmitterCount() == 0)
	{
		ImGui::TextDisabled("(No emitters)");
	}

	ImGui::Spacing();
	if (ImGui::Button("+ Add Emitter", ImVec2(-1.0f, 0.0f))) { showAddEmitterDialog_ = true; }

	// 構造変更はループ外で行う
	if (pendingOp == EmitterOp::Duplicate)
	{
		DuplicateEmitter(pendingIndex);
	}
	else if (pendingOp == EmitterOp::Delete)
	{
		currentEffect_->RemoveEmitter(pendingIndex);
		// 他エミッターからのindex参照を詰める
		const int removed = static_cast<int>(pendingIndex);
		for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
		{
			auto* other = currentEffect_->GetEmitter(i);
			if (!other) continue;
			const int follow = other->GetFollowEmitterIndex();
			if (follow == removed) other->SetFollowEmitterIndex(-1);
			else if (follow > removed) other->SetFollowEmitterIndex(follow - 1);
			const int source = other->GetGPUEventSourceEmitterIndex();
			if (source == removed) other->SetGPUEventSourceEmitterIndex(-1);
			else if (source > removed) other->SetGPUEventSourceEmitterIndex(source - 1);
		}
		if (selectedEmitterIndex_ == removed)
		{
			selectedEmitterIndex_ = -1;
			selectedModule_ = nullptr;
			inspectorTarget_ = InspectorTarget::Effect;
		}
		else if (selectedEmitterIndex_ > removed)
		{
			selectedEmitterIndex_--;
		}
		MarkDirty();
	}
}

void ParticleEditor::DrawModuleStack(ParticleEmitter* emitter, size_t emitterIndex)
{
	const bool gpuEmitter = emitter->GetSimulationMode() == SimulationMode::GPU;
	int moduleToDelete = -1;
	int moduleToMoveUp = -1;
	int moduleToMoveDown = -1;

	auto selectModule = [&](IModule* module, InspectorTarget target)
	{
		selectedEmitterIndex_ = static_cast<int>(emitterIndex);
		selectedModule_ = module;
		inspectorTarget_ = target;
	};

	auto drawPhase = [&](ModulePhase phase, const char* title, const ImVec4& color)
	{
		ImGui::TextColored(color, "%s", title);
		bool any = false;
		for (size_t i = 0; i < emitter->GetModuleCount(); ++i)
		{
			auto* module = emitter->GetModule(i);
			if (!module || module->GetPhase() != phase) continue;
			any = true;
			ImGui::PushID(module);

			bool enabled = module->IsEnabled();
			if (ImGui::Checkbox("##Enabled", &enabled))
			{
				emitter->SetModuleEnabled(i, enabled);
				MarkDirty();
			}
			ImGui::SetItemTooltip(enabled ? "有効（クリックで無効化）" : "無効（クリックで有効化）");
			ImGui::SameLine();

			const auto* descriptor = ModuleDescriptorRegistry::GetInstance().Find(module->GetName());
			const char* displayName = descriptor ? descriptor->displayName.c_str() : module->GetName();
			const bool isSelected = inspectorTarget_ == InspectorTarget::Module && selectedModule_ == module;
			// Pure GPU経路を塞ぐモジュールには印をつける
			const bool blocksPureGpu = gpuEmitter && enabled && (!descriptor || !descriptor->pureGpuSupported);
			const float tagWidth = blocksPureGpu ? ImGui::CalcTextSize("CPU").x + ImGui::GetStyle().ItemSpacing.x : 0.0f;
			const float buttonWidth = ImGui::CalcTextSize("x").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;

			if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			if (ImGui::Selectable(displayName, isSelected, ImGuiSelectableFlags_AllowOverlap,
				ImVec2((std::max)(ImGui::GetContentRegionAvail().x - tagWidth - buttonWidth, 1.0f), 0.0f)))
			{
				selectModule(module, InspectorTarget::Module);
			}
			if (!enabled) ImGui::PopStyleColor();
			if (ImGui::BeginPopupContextItem("ModuleContext"))
			{
				if (ImGui::MenuItem("Move Up", nullptr, false, i > 0)) moduleToMoveUp = static_cast<int>(i);
				if (ImGui::MenuItem("Move Down", nullptr, false, i + 1 < emitter->GetModuleCount())) moduleToMoveDown = static_cast<int>(i);
				ImGui::TextDisabled("(実行順は優先度が同じモジュール間でのみ変わります)");
				ImGui::Separator();
				if (ImGui::MenuItem("Delete")) moduleToDelete = static_cast<int>(i);
				ImGui::EndPopup();
			}
			if (blocksPureGpu)
			{
				ImGui::SameLine();
				ImGui::TextColored(kWarningColor, "CPU");
				ImGui::SetItemTooltip("このモジュールはPure GPU非対応のため、エミッター全体がCPU実行になります");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) moduleToDelete = static_cast<int>(i);
			ImGui::SetItemTooltip("Delete");
			ImGui::PopID();
		}
		if (!any) ImGui::TextDisabled("  (none)");
	};

	drawPhase(ModulePhase::Spawn, "Spawn", kSpawnColor);
	drawPhase(ModulePhase::Update, "Update", kUpdateColor);

	if (ImGui::SmallButton("+ Module"))
	{
		selectedEmitterIndex_ = static_cast<int>(emitterIndex);
		showAddModuleDialog_ = true;
	}

	// レンダラー行
	if (auto* renderer = emitter->GetRenderer())
	{
		const char* typeNames[] = { "Sprite", "Trail", "Mesh" };
		const int type = (std::clamp)(static_cast<int>(renderer->GetType()), 0, 2);
		char label[64];
		snprintf(label, sizeof(label), "Renderer: %s", typeNames[type]);
		ImGui::PushStyleColor(ImGuiCol_Text, kRendererColor);
		const bool isSelected = inspectorTarget_ == InspectorTarget::Renderer && selectedEmitterIndex_ == static_cast<int>(emitterIndex);
		if (ImGui::Selectable(label, isSelected)) selectModule(nullptr, InspectorTarget::Renderer);
		ImGui::PopStyleColor();
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Text, kWarningColor);
		if (ImGui::Selectable("Renderer: (none)")) selectModule(nullptr, InspectorTarget::Renderer);
		ImGui::PopStyleColor();
	}

	if (moduleToMoveUp >= 0) { emitter->MoveModuleUp(static_cast<size_t>(moduleToMoveUp)); MarkDirty(); }
	if (moduleToMoveDown >= 0) { emitter->MoveModuleDown(static_cast<size_t>(moduleToMoveDown)); MarkDirty(); }
	if (moduleToDelete >= 0)
	{
		if (emitter->GetModule(static_cast<size_t>(moduleToDelete)) == selectedModule_)
		{
			selectedModule_ = nullptr;
			if (inspectorTarget_ == InspectorTarget::Module) inspectorTarget_ = InspectorTarget::Emitter;
		}
		emitter->RemoveModule(static_cast<size_t>(moduleToDelete));
		MarkDirty();
	}
}

void ParticleEditor::DrawInspector()
{
	// インスペクタ内の値編集を検知して未保存フラグを立てる
	ImGuiContext& context = *ImGui::GetCurrentContext();
	const bool editedBefore = context.ActiveIdHasBeenEditedThisFrame;

	auto* emitter = GetSelectedEmitter();
	switch (inspectorTarget_)
	{
	case InspectorTarget::Effect:
		ImGui::SeparatorText("Effect");
		DrawEffectPanel();
		break;
	case InspectorTarget::Emitter:
		if (!emitter) { ImGui::TextDisabled("(No emitter selected)"); break; }
		ImGui::SeparatorText(("Emitter: " + emitter->GetName()).c_str());
		DrawEmitterPanel();
		break;
	case InspectorTarget::Module:
	{
		auto* module = emitter ? GetSelectedModule(emitter) : nullptr;
		if (!module) { ImGui::TextDisabled("(No module selected)"); break; }
		ImGui::TextDisabled("%s >", emitter->GetName().c_str());
		ImGui::SameLine();
		ImGui::TextColored(module->GetPhase() == ModulePhase::Spawn ? kSpawnColor : kUpdateColor, "%s", module->GetName());
		const auto info = GetModuleInfo(module->GetName());
		if (info.description[0] != '\0') ImGui::TextWrapped("%s", info.description);
		bool enabled = module->IsEnabled();
		for (size_t i = 0; i < emitter->GetModuleCount(); ++i)
		{
			if (emitter->GetModule(i) != module) continue;
			if (ImGui::Checkbox("Enabled##ModuleEnabled", &enabled)) emitter->SetModuleEnabled(i, enabled);
			break;
		}
		ImGui::Separator();
		ImGui::BeginDisabled(!enabled);
		DrawModuleProperties(module);
		ImGui::EndDisabled();

		// このモジュールに追加できるDynamic Input
		if (const auto* descriptor = ModuleDescriptorRegistry::GetInstance().Find(module->GetName()))
		{
			bool headerShown = false;
			for (const auto& schema : descriptor->parameters)
			{
				if (!schema.dynamicInput || emitter->FindDynamicBinding(descriptor->id, schema.id)) continue;
				if (!headerShown) { ImGui::SeparatorText("Dynamic Inputs"); headerShown = true; }
				if (ImGui::SmallButton(("+ " + schema.id).c_str()))
				{
					DynamicParameterBinding binding; binding.moduleId = descriptor->id; binding.parameterId = schema.id;
					binding.type = schema.type; binding.fallback = schema.defaultValue; binding.minimum = schema.defaultValue; binding.maximum = schema.defaultValue;
					emitter->SetDynamicBinding(std::move(binding));
					MarkDirty();
				}
				ImGui::SameLine();
			}
			if (headerShown)
			{
				ImGui::NewLine();
				ImGui::TextDisabled("追加したバインディングはエミッターのDynamic Inputsで編集します");
			}
		}
		break;
	}
	case InspectorTarget::Renderer:
		if (!emitter) { ImGui::TextDisabled("(No emitter selected)"); break; }
		ImGui::TextDisabled("%s >", emitter->GetName().c_str());
		ImGui::SameLine();
		ImGui::TextColored(kRendererColor, "Renderer");
		ImGui::Separator();
		DrawRendererPanel();
		break;
	}

	if (!editedBefore && context.ActiveIdHasBeenEditedThisFrame) MarkDirty();
}

void ParticleEditor::DrawEffectPanel()
{
	if (!currentEffect_) return;

	// エフェクト名
	ImGui::SetNextItemWidth(-80.0f);
	if (ImGui::InputText("Name", effectNameBuffer_, sizeof(effectNameBuffer_)))
	{
		currentEffect_->SetName(effectNameBuffer_);
	}

	// 位置
	Vector3 pos = currentEffect_->GetPosition();
	if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
	{
		currentEffect_->SetPosition(pos);
	}

	int deltaType = static_cast<int>(currentEffect_->GetDeltaTimeType());
	const char* deltaTypeNames[] = { "DeltaTime", "RealDeltaTime" };
	if (ImGui::Combo("Time", &deltaType, deltaTypeNames, 2))
	{
		currentEffect_->SetDeltaTimeType(static_cast<DeltaTimeType>(deltaType));
	}
	ImGui::SetItemTooltip("RealDeltaTime: ゲームのタイムスケール（ヒットストップ等）を無視する");

	ImGui::Separator();
	ImGui::Text("Emitters: %d", static_cast<int>(currentEffect_->GetEmitterCount()));
	ImGui::TextUnformatted("Path:");
	ImGui::SameLine();
	if (effectPath_.empty()) ImGui::TextDisabled("(not saved)");
	else ImGui::TextWrapped("%s", effectPath_.c_str());

	if (ImGui::TreeNode("Runtime Diagnostics"))
	{
		const auto& runtime = ParticleDiagnostics::GetInstance()->GetRuntimeCounters();
		ImGui::Text("Pure GPU emitters: %llu", static_cast<unsigned long long>(runtime.pureGpuEmitters));
		ImGui::Text("Hybrid GPU emitters: %llu", static_cast<unsigned long long>(runtime.hybridGpuEmitters));
		ImGui::Text("Packed module ops: %llu", static_cast<unsigned long long>(runtime.moduleOperations));
		ImGui::Text("Curve/gradient LUT samples: %llu", static_cast<unsigned long long>(runtime.lutSamples));
		ImGui::Text("Estimated descriptors: %llu / %u", static_cast<unsigned long long>(runtime.estimatedDescriptors), SrvManager::kMaxSRVCount);
		ImGui::TreePop();
	}
}

void ParticleEditor::DrawEmitterPanel()
{
	auto* emitter = GetSelectedEmitter();
	if (!emitter) return;

	//===== 基本設定 =====//
	if (ImGui::CollapsingHeader("Basic", ImGuiTreeNodeFlags_DefaultOpen))
	{
		char nameBuffer[256] = {};
		strncpy_s(nameBuffer, emitter->GetName().c_str(), _TRUNCATE);
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue) && nameBuffer[0] != '\0')
		{
			emitter->SetName(nameBuffer);
			MarkDirty();
		}
		ImGui::SetItemTooltip("Enterで確定");

		Vector3 pos = emitter->GetPosition();
		if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
		{
			emitter->SetPosition(pos);
		}

		int maxP = static_cast<int>(emitter->GetMaxParticles());
		if (ImGui::InputInt("Max Particles", &maxP, 100, 1000))
		{
			emitter->SetMaxParticles(static_cast<uint32_t>((std::clamp)(maxP, 1, 1000000)));
		}

		int mode = static_cast<int>(emitter->GetSimulationMode());
		const char* modes[] = { "CPU", "GPU" };
		if (ImGui::Combo("Simulation Mode", &mode, modes, IM_ARRAYSIZE(modes)))
		{
			emitter->SetSimulationMode(static_cast<SimulationMode>(mode));
		}
		if (emitter->GetSimulationMode() == SimulationMode::GPU)
		{
			// Pure GPUを塞いでいるモジュールを列挙する
			std::string blockers;
			for (size_t i = 0; i < emitter->GetModuleCount(); ++i)
			{
				const auto* module = emitter->GetModule(i);
				if (!module || !module->IsEnabled()) continue;
				const auto* descriptor = ModuleDescriptorRegistry::GetInstance().Find(module->GetName());
				if (descriptor && descriptor->pureGpuSupported) continue;
				if (!blockers.empty()) blockers += ", ";
				blockers += module->GetName();
			}
			if (!blockers.empty())
			{
				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextColored(kWarningColor, "CPU fallback: %s", blockers.c_str());
				ImGui::PopTextWrapPos();
			}
		}

		const char* spaces[] = { "World", "Local" };
		int space = static_cast<int>(emitter->GetSimulationSpace());
		if (ImGui::Combo("Simulation Space", &space, spaces, 2))
		{
			emitter->SetSimulationSpace(static_cast<SimulationSpace>(space));
		}
	}

	//===== ライフサイクル =====//
	if (ImGui::CollapsingHeader("Lifecycle", ImGuiTreeNodeFlags_DefaultOpen))
	{
		float duration = emitter->GetDuration();
		if (ImGui::DragFloat("Duration", &duration, 0.1f, 0.0f, 100.0f))
		{
			emitter->SetDuration(duration);
			// エディタ用: 設定変更時に停止中なら再開
			if (!emitter->IsEmitting()) emitter->Reset();
		}
		ImGui::SetItemTooltip("Emitter lifetime in seconds (0 = infinite)");

		float startDelay = emitter->GetStartDelay();
		if (ImGui::DragFloat("Start Delay", &startDelay, 0.1f, 0.0f, 10.0f))
		{
			emitter->SetStartDelay(startDelay);
		}
		ImGui::SetItemTooltip("Delay before particles start spawning");

		const char* loopBehaviors[] = { "Once", "Infinite", "Multiple" };
		int loopBehavior = static_cast<int>(emitter->GetLoopBehavior());
		if (ImGui::Combo("Loop Behavior", &loopBehavior, loopBehaviors, 3))
		{
			emitter->SetLoopBehavior(static_cast<LoopBehavior>(loopBehavior));
			if (!emitter->IsEmitting()) emitter->Reset();
		}
		ImGui::SetItemTooltip("Once: Play once and stop. Infinite: Loop forever. Multiple: Loop N times.");

		if (emitter->GetLoopBehavior() == LoopBehavior::Multiple)
		{
			int loopCount = emitter->GetLoopCount();
			if (ImGui::InputInt("Loop Count", &loopCount))
			{
				emitter->SetLoopCount((std::max)(1, loopCount));
				if (!emitter->IsEmitting()) emitter->Reset();
			}
		}

		const char* inactiveResponses[] = { "Complete", "Kill" };
		int inactiveResponse = static_cast<int>(emitter->GetInactiveResponse());
		if (ImGui::Combo("When Inactive", &inactiveResponse, inactiveResponses, 2))
		{
			emitter->SetInactiveResponse(static_cast<InactiveResponse>(inactiveResponse));
		}
		ImGui::SetItemTooltip("Complete: Let particles finish. Kill: Remove immediately.");

		// 持続時間に対する進行度
		if (emitter->GetDuration() > 0.0f)
		{
			char overlay[64];
			snprintf(overlay, sizeof(overlay), "%.2f / %.2f s", emitter->GetEmitterAge(), emitter->GetDuration());
			ImGui::ProgressBar((std::clamp)(emitter->GetEmitterAge() / emitter->GetDuration(), 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), overlay);
		}
		else
		{
			ImGui::TextDisabled("Age: %.2f s", emitter->GetEmitterAge());
		}
		ImGui::TextColored(emitter->IsEmitting() ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
			"Emitting: %s", emitter->IsEmitting() ? "Yes" : "No");
		ImGui::SameLine();
		if (ImGui::SmallButton("Restart")) emitter->Reset();
	}

	//===== 生成条件 =====//
	if (ImGui::CollapsingHeader("Spawn Conditions"))
	{
		bool spawnOnlyWhenMoving = emitter->GetSpawnOnlyWhenMoving();
		if (ImGui::Checkbox("Spawn Only When Moving", &spawnOnlyWhenMoving))
		{
			emitter->SetSpawnOnlyWhenMoving(spawnOnlyWhenMoving);
		}
		ImGui::SetItemTooltip("Only spawn particles when the emitter is moving (good for trails)");
		if (spawnOnlyWhenMoving)
		{
			float minDist = emitter->GetMinMoveDistance();
			if (ImGui::DragFloat("Min Move Distance", &minDist, 0.01f, 0.001f, 1.0f))
			{
				emitter->SetMinMoveDistance(minDist);
			}
			ImGui::SetItemTooltip("Minimum distance the emitter must move to spawn particles");
		}
	}

	//===== 追従・GPUイベント =====//
	if (ImGui::CollapsingHeader("Follow / GPU Events"))
	{
		Vector3 offset = emitter->GetFollowOffset();
		if (ImGui::DragFloat3("Follow Offset", &offset.x, 0.1f))
		{
			emitter->SetFollowOffset(offset);
		}
		ImGui::SetItemTooltip("Offset applied when following a target Transform or Emitter");

		// 同じエフェクト内の別エミッターを追従（自分自身は除外）
		std::vector<const char*> emitterNames{ "None" };
		std::vector<int> emitterIndices{ -1 };
		for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
		{
			if (static_cast<int>(i) == selectedEmitterIndex_) continue;
			if (auto* e = currentEffect_->GetEmitter(i))
			{
				emitterNames.push_back(e->GetName().c_str());
				emitterIndices.push_back(static_cast<int>(i));
			}
		}
		int currentSelection = 0;
		for (size_t i = 0; i < emitterIndices.size(); ++i)
		{
			if (emitterIndices[i] == emitter->GetFollowEmitterIndex()) { currentSelection = static_cast<int>(i); break; }
		}
		if (ImGui::Combo("Follow Emitter", &currentSelection, emitterNames.data(), static_cast<int>(emitterNames.size())))
		{
			emitter->SetFollowEmitterIndex(emitterIndices[currentSelection]);
		}
		ImGui::SetItemTooltip("Select another emitter to follow within this effect");

		ImGui::SeparatorText("Pure GPU Event Source");
		std::vector<const char*> eventSourceNames{ "None" };
		std::vector<int> eventSourceIndices{ -1 };
		for (int i = 0; i < selectedEmitterIndex_; ++i)
		{
			if (auto* source = currentEffect_->GetEmitter(static_cast<size_t>(i)))
			{
				eventSourceNames.push_back(source->GetName().c_str());
				eventSourceIndices.push_back(i);
			}
		}
		int eventSourceSelection = 0;
		for (size_t i = 0; i < eventSourceIndices.size(); ++i)
		{
			if (eventSourceIndices[i] == emitter->GetGPUEventSourceEmitterIndex()) eventSourceSelection = static_cast<int>(i);
		}
		if (ImGui::Combo("Event Source", &eventSourceSelection, eventSourceNames.data(), static_cast<int>(eventSourceNames.size())))
		{
			emitter->SetGPUEventSourceEmitterIndex(eventSourceIndices[eventSourceSelection]);
		}
		ImGui::SetItemTooltip("このエミッターより前のエミッターのみ選択できます");
		if (emitter->GetGPUEventSourceEmitterIndex() >= 0)
		{
			const char* triggers[] = { "On Spawn", "On Death" };
			int trigger = static_cast<int>(emitter->GetGPUEventTrigger());
			if (trigger < 0 || trigger > 1) trigger = 1;
			if (ImGui::Combo("Event Trigger", &trigger, triggers, 2)) emitter->SetGPUEventTrigger(static_cast<uint32_t>(trigger));
			ImGui::TextDisabled("Collision GPU events are unavailable until a GPU collision producer is enabled.");
			float probability = emitter->GetGPUEventProbability();
			if (ImGui::SliderFloat("Probability", &probability, 0.0f, 1.0f)) emitter->SetGPUEventProbability(probability);
			bool inheritVelocity = emitter->GetGPUEventInheritVelocity();
			float velocityScale = emitter->GetGPUEventVelocityScale();
			if (ImGui::Checkbox("Inherit Velocity", &inheritVelocity) ||
				(inheritVelocity && ImGui::DragFloat("Velocity Scale", &velocityScale, 0.05f)))
			{
				emitter->SetGPUEventVelocityInheritance(inheritVelocity, velocityScale);
			}
			bool inheritColor = emitter->GetGPUEventInheritColor();
			if (ImGui::Checkbox("Inherit Color", &inheritColor)) emitter->SetGPUEventInheritColor(inheritColor);
		}
	}

	DrawEmitterParameterPanel(emitter);
}

void ParticleEditor::DrawEmitterParameterPanel(ParticleEmitter* emitter)
{
	if (ImGui::CollapsingHeader("Emitter Parameters"))
	{
		auto& values = emitter->GetParameterStore().GetValues();
		std::string removeName;
		for (auto& [name, value] : values)
		{
			ImGui::PushID(name.c_str());
			ImGui::TextUnformatted(name.c_str()); ImGui::SameLine(180.0f);
			ImGui::SetNextItemWidth(-30.0f);
			std::visit([&](auto& typedValue)
			{
				using T = std::decay_t<decltype(typedValue)>;
				if constexpr (std::is_same_v<T, float>) ImGui::DragFloat("##value", &typedValue, 0.01f);
				else if constexpr (std::is_same_v<T, uint32_t>) { int v = static_cast<int>(typedValue); if (ImGui::InputInt("##value", &v)) typedValue = static_cast<uint32_t>((std::max)(0, v)); }
				else if constexpr (std::is_same_v<T, int32_t>) ImGui::InputInt("##value", &typedValue);
				else if constexpr (std::is_same_v<T, bool>) ImGui::Checkbox("##value", &typedValue);
				else if constexpr (std::is_same_v<T, Vector3>) ImGui::DragFloat3("##value", &typedValue.x, 0.01f);
				else if constexpr (std::is_same_v<T, Vector4>) ImGui::DragFloat4("##value", &typedValue.x, 0.01f);
				else if constexpr (std::is_same_v<T, std::string>) ImGui::TextDisabled("%s", typedValue.c_str());
			}, value);
			ImGui::SameLine(); if (ImGui::SmallButton("x")) removeName = name;
			ImGui::PopID();
		}
		if (!removeName.empty()) emitter->GetParameterStore().Remove(removeName);
		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputText("##newParameter", newParameterNameBuffer_, sizeof(newParameterNameBuffer_)); ImGui::SameLine();
		const bool canAdd = newParameterNameBuffer_[0] != '\0';
		if (ImGui::Button("Float") && canAdd) emitter->GetParameterStore().Set(newParameterNameBuffer_, 0.0f);
		ImGui::SameLine(); if (ImGui::Button("Vector3") && canAdd) emitter->GetParameterStore().Set(newParameterNameBuffer_, Vector3{});
		ImGui::SameLine(); if (ImGui::Button("Color") && canAdd) emitter->GetParameterStore().Set(newParameterNameBuffer_, Vector4{ 1, 1, 1, 1 });
	}
	if (ImGui::CollapsingHeader("Dynamic Inputs"))
	{
		std::string removeModule, removeParameter;
		for (const auto& stored : emitter->GetDynamicBindings())
		{
			DynamicParameterBinding binding = stored;
			ImGui::PushID((binding.moduleId + "." + binding.parameterId).c_str());
			ImGui::SeparatorText((binding.moduleId + "." + binding.parameterId).c_str());
			const char* modes[] = { "Constant", "Random Range", "Curve", "Emitter Parameter" };
			int mode = static_cast<int>(binding.mode);
			bool changed = ImGui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes));
			binding.mode = static_cast<DynamicBindingMode>(mode);
			if (binding.mode == DynamicBindingMode::EmitterParameter)
			{
				std::vector<std::string> candidates;
				for (const auto& [name, parameterValue] : emitter->GetParameterStore().GetValues())
					if (parameterValue.index() == binding.fallback.index()) candidates.push_back(name);
				std::sort(candidates.begin(), candidates.end());
				const char* preview = binding.emitterParameter.empty() ? "(Select Parameter)" : binding.emitterParameter.c_str();
				if (ImGui::BeginCombo("Parameter", preview))
				{
					for (const auto& candidate : candidates) if (ImGui::Selectable(candidate.c_str(), candidate == binding.emitterParameter)) { binding.emitterParameter = candidate; changed = true; }
					ImGui::EndCombo();
				}
			}
			else if (binding.mode == DynamicBindingMode::RandomRange)
			{
				std::visit([&](auto& minimum)
				{
					using T = std::decay_t<decltype(minimum)>;
					if (auto* maximum = std::get_if<T>(&binding.maximum))
					{
						if constexpr (std::is_same_v<T, float>) { changed |= ImGui::DragFloat("Minimum", &minimum, 0.01f); changed |= ImGui::DragFloat("Maximum", maximum, 0.01f); }
						else if constexpr (std::is_same_v<T, Vector3>) { changed |= ImGui::DragFloat3("Minimum", &minimum.x, 0.01f); changed |= ImGui::DragFloat3("Maximum", &maximum->x, 0.01f); }
						else if constexpr (std::is_same_v<T, Vector4>) { changed |= ImGui::DragFloat4("Minimum", &minimum.x, 0.01f); changed |= ImGui::DragFloat4("Maximum", &maximum->x, 0.01f); }
					}
				}, binding.minimum);
			}
			else if (binding.mode == DynamicBindingMode::Curve)
			{
				// カーブ形状のプレビュー（Float型のみ）
				if (!binding.keys.empty() && binding.type == ModuleParameterType::Float)
				{
					DrawCurvePlot("##BindingCurve", [&](float t)
					{
						const ModuleParameterValue value = binding.Evaluate(t, 0, nullptr);
						const float* typed = std::get_if<float>(&value);
						return typed ? *typed : 0.0f;
					});
				}
				int removeKey = -1;
				for (size_t keyIndex = 0; keyIndex < binding.keys.size(); ++keyIndex)
				{
					ImGui::PushID(static_cast<int>(keyIndex));
					changed |= ImGui::SliderFloat("Time", &binding.keys[keyIndex].time, 0.0f, 1.0f);
					if (binding.type == ModuleParameterType::Float) changed |= ImGui::DragFloat("Value", &binding.keys[keyIndex].value.x, 0.01f);
					else if (binding.type == ModuleParameterType::Vector3) changed |= ImGui::DragFloat3("Value", &binding.keys[keyIndex].value.x, 0.01f);
					else if (binding.type == ModuleParameterType::Vector4) changed |= ImGui::DragFloat4("Value", &binding.keys[keyIndex].value.x, 0.01f);
					if (ImGui::SmallButton("Delete Key")) removeKey = static_cast<int>(keyIndex);
					ImGui::PopID();
				}
				if (removeKey >= 0) { binding.keys.erase(binding.keys.begin() + removeKey); changed = true; }
				if (binding.keys.size() < 1024 && ImGui::Button("Add Key")) { binding.keys.push_back({ binding.keys.empty() ? 0.0f : 1.0f, {} }); changed = true; }
			}
			else
			{
				std::visit([&](auto& fallback)
				{
					using T = std::decay_t<decltype(fallback)>;
					if constexpr (std::is_same_v<T, float>) changed |= ImGui::DragFloat("Value", &fallback, 0.01f);
					else if constexpr (std::is_same_v<T, Vector3>) changed |= ImGui::DragFloat3("Value", &fallback.x, 0.01f);
					else if constexpr (std::is_same_v<T, Vector4>) changed |= ImGui::DragFloat4("Value", &fallback.x, 0.01f);
				}, binding.fallback);
			}
			if (changed) emitter->SetDynamicBinding(std::move(binding));
			if (ImGui::SmallButton("Remove Binding")) { removeModule = stored.moduleId; removeParameter = stored.parameterId; }
			ImGui::PopID();
		}
		if (!removeModule.empty()) { emitter->RemoveDynamicBinding(removeModule, removeParameter); MarkDirty(); }
		if (emitter->GetDynamicBindings().empty())
		{
			ImGui::TextDisabled("モジュールを選択するとDynamic Inputを追加できます");
		}
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
		bool changed = false;
		if (ImGui::Combo(label, &current, easingNames, IM_ARRAYSIZE(easingNames))) {
			*type = static_cast<EasingType>(current);
			changed = true;
		}
		// イージング形状のプレビュー
		const EasingType previewType = *type;
		DrawCurvePlot("##EasingPreview", [previewType](float t) { return ApplyEasing(previewType, 0.0f, 1.0f, t); }, 36.0f);
		return changed;
	};

	ImGui::Text("Module: %s", module->GetName());
	if (const auto* descriptor = ModuleDescriptorRegistry::GetInstance().Find(module->GetName()))
	{
		ImGui::SameLine();
		ImGui::TextDisabled("v%u | %s | kernel: %s", descriptor->version,
			descriptor->pureGpuSupported ? "Pure GPU" : "CPU/Hybrid",
			descriptor->kernelId.empty() ? "none" : descriptor->kernelId.c_str());
		if (!descriptor->requiredAttributes.empty() && ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted("Required attributes:");
			for (const auto& attribute : descriptor->requiredAttributes) ImGui::BulletText("%s", attribute.c_str());
			ImGui::EndTooltip();
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "Unregistered module: playback is rejected");
	}
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
	else if (auto* m = dynamic_cast<InitialPositionModule*>(module))
	{
		Vector3 minOffset = m->GetMinOffset();
		Vector3 maxOffset = m->GetMaxOffset();
		bool changed = ImGui::DragFloat3("Min Offset", &minOffset.x, 0.1f);
		changed |= ImGui::DragFloat3("Max Offset", &maxOffset.x, 0.1f);
		if (changed) m->SetOffsetRange(minOffset, maxOffset);
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
	else if (auto* m = dynamic_cast<AssignRibbonIdModule*>(module))
	{
		int groupCount = static_cast<int>(m->GetGroupCount());
		if (ImGui::InputInt("Group Count", &groupCount, 1, 5))
		{
			m->SetGroupCount(static_cast<uint32_t>((std::max)(1, groupCount)));
		}
		ImGui::SetItemTooltip("Number of ribbon groups. Each group forms a separate trail.");
	}
	else if (auto* m = dynamic_cast<GravityModule*>(module))
	{
		Vector3 minG = m->GetMinGravity();
		Vector3 maxG = m->GetMaxGravity();
		if (ImGui::DragFloat3("Min Gravity", &minG.x, 0.1f)) { m->SetGravityRange(minG, maxG); }
		if (ImGui::DragFloat3("Max Gravity", &maxG.x, 0.1f)) { m->SetGravityRange(minG, maxG); }
	}
	else if (auto* m = dynamic_cast<DragModule*>(module))
	{
		float minD = m->GetMinDrag();
		float maxD = m->GetMaxDrag();
		if (ImGui::DragFloat("Min Drag", &minD, 0.01f, 0.0f, 10.0f)) { m->SetDragRange(minD, maxD); }
		if (ImGui::DragFloat("Max Drag", &maxD, 0.01f, 0.0f, 10.0f)) { m->SetDragRange(minD, maxD); }
	}
	else if (auto* m = dynamic_cast<ColorFadeModule*>(module))
	{
		bool useGradient = m->HasGradient();
		if (ImGui::Checkbox("Use Gradient", &useGradient))
		{
			if (useGradient)
			{
				ColorGradient gradient; gradient.AddKey(0.0f, m->GetStartColor()); gradient.AddKey(1.0f, m->GetEndColor()); m->SetGradient(gradient);
			}
			else m->ClearGradient();
		}
		// 寿命に沿った色のプレビュー
		if (m->HasGradient())
		{
			DrawColorRamp([m](float t) { return m->GetGradient().Evaluate(t); });
		}
		else
		{
			const Vector4 start = m->GetUseInitialColor() ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : m->GetStartColor();
			const Vector4 end = m->GetEndColor();
			const EasingType easing = m->GetEasingType();
			DrawColorRamp([start, end, easing](float t) { return ApplyEasing(easing, start, end, t); });
			if (m->GetUseInitialColor()) ImGui::TextDisabled("(開始色はInitialColorの色。プレビューは白で表示)");
		}
		if (m->HasGradient())
		{
			auto& keys = m->GetGradient().keys;
			int removeKey = -1;
			for (size_t i = 0; i < keys.size(); ++i)
			{
				ImGui::PushID(static_cast<int>(i));
				ImGui::SetNextItemWidth(90.0f); ImGui::DragFloat("Time", &keys[i].time, 0.005f, 0.0f, 1.0f); ImGui::SameLine();
				ImGui::ColorEdit4("Color", &keys[i].color.x, ImGuiColorEditFlags_NoInputs); ImGui::SameLine();
				if (ImGui::SmallButton("x")) removeKey = static_cast<int>(i);
				ImGui::PopID();
			}
			if (removeKey >= 0 && keys.size() > 1) keys.erase(keys.begin() + removeKey);
			if (ImGui::Button("Add Gradient Key") && keys.size() < 64) m->GetGradient().AddKey(0.5f, m->GetGradient().Evaluate(0.5f));
			std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
			return;
		}
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

		EasingType easing = m->GetEasingType();
		if (DrawEasingCombo("Easing Type", &easing)) { m->SetEasingType(easing); }
	}
	else if (auto* m = dynamic_cast<ScaleOverLifetimeModule*>(module))
	{
		Vector3 start = m->GetStartScale();
		Vector3 end = m->GetEndScale();
		if (ImGui::DragFloat3("Start Scale", &start.x, 0.01f)) { m->SetStartScale(start); }
		if (ImGui::DragFloat3("End Scale", &end.x, 0.01f)) { m->SetEndScale(end); }

		EasingType easing = m->GetEasingType();
		if (DrawEasingCombo("Easing Type", &easing)) { m->SetEasingType(easing); }
		if (m->HasCurve())
		{
			// カーブ使用時は「カーブで変形した進行度」にイージングを掛けた結果（X成分）を表示
			ImGui::TextDisabled("Scale X over lifetime (curve + easing)");
			DrawCurvePlot("##ScaleCurvePreview", [m, start, end, easing](float t)
			{
				return ApplyEasing(easing, start.x, end.x, m->GetCurve().Evaluate(t));
			});
		}
		bool useCurve = m->HasCurve();
		if (ImGui::Checkbox("Use Interpolation Curve", &useCurve))
		{
			if (useCurve) { AnimationCurve curve; curve.AddKey(0.0f, 0.0f); curve.AddKey(1.0f, 1.0f); m->SetCurve(curve); }
			else m->ClearCurve();
		}
		if (m->HasCurve())
		{
			auto& keys = m->GetCurve().keys;
			int removeKey = -1;
			for (size_t i = 0; i < keys.size(); ++i)
			{
				ImGui::PushID(static_cast<int>(i));
				ImGui::SetNextItemWidth(90.0f); ImGui::DragFloat("Time", &keys[i].time, 0.005f, 0.0f, 1.0f); ImGui::SameLine();
				ImGui::SetNextItemWidth(110.0f); ImGui::DragFloat("Value", &keys[i].value, 0.01f); ImGui::SameLine();
				if (ImGui::SmallButton("x")) removeKey = static_cast<int>(i);
				ImGui::PopID();
			}
			if (removeKey >= 0 && keys.size() > 1) keys.erase(keys.begin() + removeKey);
			if (ImGui::Button("Add Curve Key") && keys.size() < 64) m->GetCurve().AddKey(0.5f, m->GetCurve().Evaluate(0.5f));
			std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
		}
	}
	else if (auto* m = dynamic_cast<RibbonInterpolationModule*>(module))
	{
		float maxDistance = m->GetMaxDistance();
		if (ImGui::DragFloat("Max Segment Distance", &maxDistance, 0.01f, 0.001f, 1000.0f))
		{
			m->SetMaxDistance((std::max)(0.001f, maxDistance));
		}
	}
	else if (auto* m = dynamic_cast<MultiSourceRibbonModule*>(module))
	{
		float spawnRate = m->GetSpawnRate();
		float lifetime = m->GetParticleLifetime();
		float minMoveDistance = m->GetMinMoveDistance();
		Vector4 color = m->GetInitialColor();
		bool onlyWhenMoving = m->GetSpawnOnlyWhenMoving();
		if (ImGui::DragFloat("Spawn Rate", &spawnRate, 0.1f, 0.0f, 100000.0f)) m->SetSpawnRate((std::max)(0.0f, spawnRate));
		if (ImGui::DragFloat("Particle Lifetime", &lifetime, 0.01f, 0.001f, 1000.0f)) m->SetParticleLifetime((std::max)(0.001f, lifetime));
		if (ImGui::ColorEdit4("Initial Color", &color.x)) m->SetInitialColor(color);
		if (ImGui::Checkbox("Spawn Only When Moving", &onlyWhenMoving)) m->SetSpawnOnlyWhenMoving(onlyWhenMoving);
		if (ImGui::DragFloat("Minimum Move Distance", &minMoveDistance, 0.001f, 0.0f, 1000.0f)) m->SetMinMoveDistance((std::max)(0.0f, minMoveDistance));
		ImGui::TextDisabled("Registered runtime sources: %llu", static_cast<unsigned long long>(m->GetSourceCount()));
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

		// 形状ごとに意味のあるパラメータだけを表示する
		const bool usesRadius = currentShape == SpawnShapeType::Sphere || currentShape == SpawnShapeType::Circle || currentShape == SpawnShapeType::Cone;
		const bool usesInnerRadius = currentShape == SpawnShapeType::Sphere || currentShape == SpawnShapeType::Circle;
		const bool usesArc = currentShape == SpawnShapeType::Circle || currentShape == SpawnShapeType::Cone;

		if (usesInnerRadius)
		{
			float innerRadius = m->GetInnerRadius();
			if (ImGui::DragFloat("Inner Radius", &innerRadius, 0.1f, 0.0f, 100.0f)) { m->SetInnerRadius(innerRadius); }
		}
		if (usesRadius)
		{
			float outerRadius = m->GetOuterRadius();
			if (ImGui::DragFloat(currentShape == SpawnShapeType::Cone ? "Base Radius" : "Outer Radius", &outerRadius, 0.1f, 0.0f, 100.0f)) { m->SetOuterRadius(outerRadius); }
		}
		if (currentShape == SpawnShapeType::Box)
		{
			Vector3 boxSize = m->GetBoxSize();
			if (ImGui::DragFloat3("Box Size", &boxSize.x, 0.1f)) { m->SetBoxSize(boxSize); }
		}
		if (currentShape == SpawnShapeType::Cone)
		{
			float coneHeight = m->GetConeHeight();
			if (ImGui::DragFloat("Cone Height", &coneHeight, 0.1f, 0.0f, 100.0f)) { m->SetConeHeight(coneHeight); }
		}
		if (currentShape == SpawnShapeType::Line)
		{
			Vector3 lineStart = m->GetLineStart();
			Vector3 lineEnd = m->GetLineEnd();
			if (ImGui::DragFloat3("Line Start", &lineStart.x, 0.1f)) { m->SetLine(lineStart, lineEnd); }
			if (ImGui::DragFloat3("Line End", &lineEnd.x, 0.1f)) { m->SetLine(lineStart, lineEnd); }
		}
		if (usesArc)
		{
			float arcAngle = m->GetArcAngle();
			if (ImGui::SliderFloat("Arc Angle", &arcAngle, 0.0f, 360.0f, "%.0f deg")) { m->SetArcAngle(arcAngle); }
			ImGui::SetItemTooltip("Spawn arc angle (0-360)");
		}
		if (currentShape != SpawnShapeType::Point && currentShape != SpawnShapeType::Line)
		{
			bool emitFromSurface = m->GetEmitFromSurface();
			if (ImGui::Checkbox("Emit From Surface", &emitFromSurface)) { m->SetEmitFromSurface(emitFromSurface); }
		}

		float initialSpeed = m->GetInitialSpeed();
		if (ImGui::DragFloat("Initial Speed", &initialSpeed, 0.1f, 0.0f, 100.0f)) { m->SetInitialSpeed(initialSpeed); }
		ImGui::SetItemTooltip("形状に沿って外向きに与える初速");
	}
	else if (auto* m = dynamic_cast<InitialRotationModule*>(module))
	{
		Vector3 minAngle = m->GetMinAngle();
		Vector3 maxAngle = m->GetMaxAngle();
		if (ImGui::DragFloat3("Min Angle", &minAngle.x, 1.0f, 0.0f, 360.0f)) { m->SetRotationRange(minAngle, maxAngle); }
		if (ImGui::DragFloat3("Max Angle", &maxAngle.x, 1.0f, 0.0f, 360.0f)) { m->SetRotationRange(minAngle, maxAngle); }
	}
	else if (auto* m = dynamic_cast<RotationOverLifetimeModule*>(module))
	{
		float startSpeed = m->GetStartSpeed();
		float endSpeed = m->GetEndSpeed();
		if (ImGui::DragFloat("Start Speed (deg/s)", &startSpeed, 1.0f, -1000.0f, 1000.0f)) { m->SetRotationSpeedRange(startSpeed, endSpeed); }
		if (ImGui::DragFloat("End Speed (deg/s)", &endSpeed, 1.0f, -1000.0f, 1000.0f)) { m->SetRotationSpeedRange(startSpeed, endSpeed); }

		EasingType easing = m->GetEasingType();
		if (DrawEasingCombo("Easing Type", &easing)) { m->SetEasingType(easing); }
	}
	else if (auto* m = dynamic_cast<FaceVelocityModule*>(module))
	{
		ImGui::Text("Aligns particle rotation to its velocity direction.");
		bool use2D = m->IsUse2DAlignment();
		if (ImGui::Checkbox("Use 2D Alignment", &use2D))
		{
			m->SetUse2DAlignment(use2D);
		}
		ImGui::SetItemTooltip("ON: 2D Rolling (for Sprites), OFF: 3D Pitch/Yaw (for Meshes)");
	}
	else if (auto* m = dynamic_cast<JitterModule*>(module))
	{
		Vector3 amount = m->GetAmount();
		if (ImGui::DragFloat3("Amount", &amount.x, 0.01f, 0.0f, 10.0f)) { m->SetAmount(amount); }
	}
	else if (auto* m = dynamic_cast<ForceOverLifetimeModule*>(module))
	{
		Vector3 dir = m->GetDirection();
		if (ImGui::DragFloat3("Direction", &dir.x, 0.1f)) { m->SetDirection(dir); }

		float startS = m->GetStartStrength();
		float endS = m->GetEndStrength();
		if (ImGui::DragFloat("Start Strength", &startS, 0.1f)) { m->SetStrengths(startS, endS); }
		if (ImGui::DragFloat("End Strength", &endS, 0.1f)) { m->SetStrengths(startS, endS); }

		EasingType easing = m->GetEasingType();
		if (DrawEasingCombo("Easing Type", &easing)) { m->SetEasingType(easing); }
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
			char pathBuffer[256] = {};
			strncpy_s(pathBuffer, config->effectPath.c_str(), _TRUNCATE);
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
	// Motion Effect Modules - Phase 5
	else if (auto* m = dynamic_cast<RadialVelocityModule*>(module))
	{
		float minSpeed = m->GetMinSpeed();
		float maxSpeed = m->GetMaxSpeed();
		if (ImGui::DragFloat("Min Speed", &minSpeed, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpeed, maxSpeed); }
		if (ImGui::DragFloat("Max Speed", &maxSpeed, 0.1f, 0.0f, 100.0f)) { m->SetSpeedRange(minSpeed, maxSpeed); }
	}
	else if (auto* m = dynamic_cast<VelocityOverLifetimeModule*>(module))
	{
		float startMul = m->GetStartMultiplier();
		float endMul = m->GetEndMultiplier();
		if (ImGui::DragFloat("Start Multiplier", &startMul, 0.01f, 0.0f, 2.0f)) { m->SetStartMultiplier(startMul); }
		if (ImGui::DragFloat("End Multiplier", &endMul, 0.01f, 0.0f, 2.0f)) { m->SetEndMultiplier(endMul); }
	}
	else if (auto* m = dynamic_cast<StretchByVelocityModule*>(module))
	{
		float factor = m->GetStretchFactor();
		if (ImGui::DragFloat("Stretch Factor", &factor, 0.01f, 0.0f, 1.0f)) { m->SetStretchFactor(factor); }
		
		float minS = m->GetMinStretch();
		float maxS = m->GetMaxStretch();
		if (ImGui::DragFloat("Min Stretch", &minS, 0.1f, 0.1f, 10.0f)) { m->SetMinStretch(minS); }
		if (ImGui::DragFloat("Max Stretch", &maxS, 0.1f, 0.1f, 10.0f)) { m->SetMaxStretch(maxS); }
		
		bool preserve = m->GetPreserveVolume();
		if (ImGui::Checkbox("Preserve Volume", &preserve)) { m->SetPreserveVolume(preserve); }
		ImGui::SetItemTooltip("Shrink X/Z when stretching Y to maintain volume");
	}
	else if (auto* m = dynamic_cast<WindModule*>(module))
	{
		Vector3 dir = m->GetDirection();
		if (ImGui::DragFloat3("Direction", &dir.x, 0.01f)) { m->SetDirection(dir); }
		
		float strength = m->GetStrength();
		if (ImGui::DragFloat("Strength", &strength, 0.1f, 0.0f, 50.0f)) { m->SetStrength(strength); }
		
		float turb = m->GetTurbulence();
		if (ImGui::DragFloat("Turbulence", &turb, 0.01f, 0.0f, 1.0f)) { m->SetTurbulence(turb); }
		
		float turbFreq = m->GetTurbulenceFrequency();
		if (ImGui::DragFloat("Turbulence Freq", &turbFreq, 0.1f, 0.1f, 10.0f)) { m->SetTurbulenceFrequency(turbFreq); }
	}
	else if (auto* m = dynamic_cast<FlickerModule*>(module))
	{
		float freq = m->GetFrequency();
		if (ImGui::DragFloat("Frequency", &freq, 0.1f, 0.1f, 50.0f)) { m->SetFrequency(freq); }
		
		float minA = m->GetMinAlpha();
		float maxA = m->GetMaxAlpha();
		if (ImGui::DragFloat("Min Alpha", &minA, 0.01f, 0.0f, 1.0f)) { m->SetMinAlpha(minA); }
		if (ImGui::DragFloat("Max Alpha", &maxA, 0.01f, 0.0f, 1.0f)) { m->SetMaxAlpha(maxA); }
		
		bool randPhase = m->GetRandomPhase();
		if (ImGui::Checkbox("Random Phase", &randPhase)) { m->SetRandomPhase(randPhase); }
		
		bool useNoise = m->GetUseNoise();
		if (ImGui::Checkbox("Use Noise", &useNoise)) { m->SetUseNoise(useNoise); }
	}
	else if (auto* m = dynamic_cast<AlphaFadeModule*>(module))
	{
		float startA = m->GetStartAlpha();
		float endA = m->GetEndAlpha();
		if (ImGui::DragFloat("Start Alpha", &startA, 0.01f, 0.0f, 1.0f)) { m->SetStartAlpha(startA); }
		if (ImGui::DragFloat("End Alpha", &endA, 0.01f, 0.0f, 1.0f)) { m->SetEndAlpha(endA); }
		
		bool easeIn = m->GetEaseIn();
		bool easeOut = m->GetEaseOut();
		if (ImGui::Checkbox("Ease In", &easeIn)) { m->SetEaseIn(easeIn); }
		ImGui::SameLine();
		if (ImGui::Checkbox("Ease Out", &easeOut)) { m->SetEaseOut(easeOut); }
	}
	else if (auto* m = dynamic_cast<RotationBySpeedModule*>(module))
	{
		float rotPerSpeed = m->GetRotationPerSpeed();
		if (ImGui::DragFloat("Rotation/Speed (deg)", &rotPerSpeed, 1.0f, -360.0f, 360.0f)) { m->SetRotationPerSpeed(rotPerSpeed); }
		
		float minSpd = m->GetMinSpeed();
		float maxSpd = m->GetMaxSpeed();
		if (ImGui::DragFloat("Min Speed", &minSpd, 0.1f, 0.0f, 100.0f)) { m->SetMinSpeed(minSpd); }
		if (ImGui::DragFloat("Max Speed (0=unlimited)", &maxSpd, 0.1f, 0.0f, 100.0f)) { m->SetMaxSpeed(maxSpd); }
	}
	else if (auto* m = dynamic_cast<SineWaveModule*>(module))
	{
		float amp = m->GetAmplitude();
		if (ImGui::DragFloat("Amplitude", &amp, 0.1f, 0.0f, 10.0f)) { m->SetAmplitude(amp); }
		
		float freq = m->GetFrequency();
		if (ImGui::DragFloat("Frequency", &freq, 0.1f, 0.1f, 20.0f)) { m->SetFrequency(freq); }
		
		Vector3 axis = m->GetAxis();
		if (ImGui::DragFloat3("Axis", &axis.x, 0.01f)) { m->SetAxis(axis); }
		
		bool randPhase = m->GetRandomPhase();
		if (ImGui::Checkbox("Random Phase", &randPhase)) { m->SetRandomPhase(randPhase); }
	}
	else if (auto* m = dynamic_cast<SpiralModule*>(module))
	{
		float radius = m->GetRadius();
		if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.01f, 10.0f)) { m->SetRadius(radius); }
		
		float speed = m->GetSpeed();
		if (ImGui::DragFloat("Speed (deg/s)", &speed, 1.0f, -720.0f, 720.0f)) { m->SetSpeed(speed); }
		
		float lift = m->GetLift();
		if (ImGui::DragFloat("Lift", &lift, 0.1f, -10.0f, 10.0f)) { m->SetLift(lift); }
		
		bool randPhase = m->GetRandomPhase();
		if (ImGui::Checkbox("Random Phase", &randPhase)) { m->SetRandomPhase(randPhase); }
		
		bool expand = m->GetExpandRadius();
		if (ImGui::Checkbox("Expand Radius", &expand)) { m->SetExpandRadius(expand); }
		
		if (expand)
		{
			float expRate = m->GetExpansionRate();
			if (ImGui::DragFloat("Expansion Rate", &expRate, 0.1f, 0.0f, 5.0f)) { m->SetExpansionRate(expRate); }
		}
	}
	else if (auto* m = dynamic_cast<TwistModule*>(module))
	{
		float twistSpeed = m->GetTwistSpeed();
		if (ImGui::DragFloat("Twist Speed (deg/s)", &twistSpeed, 1.0f, -360.0f, 360.0f)) { m->SetTwistSpeed(twistSpeed); }
		
		float twistStrength = m->GetTwistStrength();
		if (ImGui::DragFloat("Twist Strength", &twistStrength, 0.1f, 0.0f, 10.0f)) { m->SetTwistStrength(twistStrength); }
		
		const char* axes[] = { "X", "Y", "Z" };
		int axis = m->GetHeightAxis();
		if (ImGui::Combo("Height Axis", &axis, axes, 3)) { m->SetHeightAxis(axis); }
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

	
	{
		auto* renderer = emitter->GetRenderer();
		if (renderer)
		{
			const char* typeNames[] = { "Sprite", "Trail", "Mesh" };
			int currentType = (std::clamp)(static_cast<int>(renderer->GetType()), 0, 2);
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
			int currentBlend = (std::clamp)(static_cast<int>(renderer->GetBlendMode()), 0, IM_ARRAYSIZE(blendModes) - 1);
			if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, IM_ARRAYSIZE(blendModes)))
			{
				renderer->SetBlendMode(static_cast<BlendMode>(currentBlend));
			}

			ImGui::SeparatorText("Selective Bloom");
			EmissiveSettings emissive = renderer->GetEmissiveSettings();
			if (ImGui::Checkbox("Enabled##Emissive", &emissive.enabled))
			{
				renderer->SetEmissiveEnabled(emissive.enabled);
				// Emissiveを有効化した際は、ポストプロセス側の選択的ブルームも自動で有効化する
				// （個別にEmissiveをONにしても、選択的ブルームがOFFのままだと光らないため）
				if (emissive.enabled && postProcessManager_)
				{
					postProcessManager_->SetSelectiveBloomModeEnabled(true);
				}
			}
			const char* emissiveSources[] = { "Uniform", "Base Texture Mask", "Emissive Texture" };
			int emissiveSource = static_cast<int>(emissive.source);
			if (ImGui::Combo("Source##Emissive", &emissiveSource, emissiveSources, IM_ARRAYSIZE(emissiveSources)))
			{
				renderer->SetEmissiveSource(static_cast<EmissiveSource>(emissiveSource));
			}
			float emissiveColor[3] = { emissive.color.x, emissive.color.y, emissive.color.z };
			if (ImGui::ColorEdit3("Color##Emissive", emissiveColor, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float))
			{
				renderer->SetEmissiveColor({ emissiveColor[0], emissiveColor[1], emissiveColor[2] });
			}
			if (ImGui::DragFloat("Intensity##Emissive", &emissive.intensity, 0.05f, 0.0f, 64.0f)) renderer->SetEmissiveIntensity(emissive.intensity);
			if (ImGui::SliderFloat("Bloom Contribution##Emissive", &emissive.bloomContribution, 0.0f, 1.0f)) renderer->SetBloomContribution(emissive.bloomContribution);
			if (emissive.source == EmissiveSource::EmissiveTexture)
			{
				std::string emissivePath = renderer->GetEmissiveTexturePath();
				char emissivePathBuffer[512] = {};
				strncpy_s(emissivePathBuffer, emissivePath.c_str(), _TRUNCATE);
				if (ImGui::InputText("Texture##Emissive", emissivePathBuffer, sizeof(emissivePathBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					renderer->SetEmissiveTexture(emissivePathBuffer);
				}
				ImGui::TextDisabled("Linear color; empty or load failure uses black.");
			}

			// Sprite Renderer特有の設定
			if (auto* spriteRenderer = dynamic_cast<SpriteRenderer*>(renderer))
			{
				std::string selectedTexture;
				if (DrawTextureCombo("##spriteRendererTexture", spriteRenderer->GetTexturePath(), selectedTexture)) spriteRenderer->SetTexture(selectedTexture);

				// ティントカラー
				Vector4 tintColor = spriteRenderer->GetTintColor();
				if (ImGui::ColorEdit4("Tint Color##Sprite", &tintColor.x))
				{
					spriteRenderer->SetTintColor(tintColor);
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

				std::string selectedTexture;
				if (DrawTextureCombo("##trailRendererTexture", trailRenderer->GetTexturePath(), selectedTexture)) trailRenderer->SetTexture(selectedTexture);

				// ティントカラー
				Vector4 tintColor = trailRenderer->GetTintColor();
				if (ImGui::ColorEdit4("Tint Color##Trail", &tintColor.x))
				{
					trailRenderer->SetTintColor(tintColor);
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

				// Cube Options
				if (currentType == PrimitiveType::Cube)
				{
					if (ImGui::DragFloat3("Size (XYZ)", &options.cubeSize.x, 0.01f, 0.0f, 100.0f))
					{
						optionsChanged = true;
					}

					ImGui::Text("Visible Faces:");
					// 2列で表示
					if (ImGui::Checkbox("Front##Cube", &options.cubeFaceVisible[0])) optionsChanged = true;
					ImGui::SameLine();
					if (ImGui::Checkbox("Back##Cube", &options.cubeFaceVisible[1])) optionsChanged = true;

					if (ImGui::Checkbox("Top##Cube", &options.cubeFaceVisible[2])) optionsChanged = true;
					ImGui::SameLine();
					if (ImGui::Checkbox("Bottom##Cube", &options.cubeFaceVisible[3])) optionsChanged = true;

					if (ImGui::Checkbox("Right##Cube", &options.cubeFaceVisible[4])) optionsChanged = true;
					ImGui::SameLine();
					if (ImGui::Checkbox("Left##Cube", &options.cubeFaceVisible[5])) optionsChanged = true;
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

				std::string selectedTexture;
				if (DrawTextureCombo("##meshRendererTexture", meshRenderer->GetTexturePath(), selectedTexture)) meshRenderer->SetTexture(selectedTexture);
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

void ParticleEditor::AddEmitterDialog()
{
	ImGui::OpenPopup("Add Emitter");

	if (ImGui::BeginPopupModal("Add Emitter", &showAddEmitterDialog_, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(220.0f);
		const bool submitted = ImGui::InputTextWithHint("Name", "Emitter", emitterNameBuffer_, sizeof(emitterNameBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);

		const char* templates[] = { "Empty", "Basic (continuous)", "Burst (explosion)", "Smoke (rising)", "Trail" };
		const char* templateDescriptions[] = {
			"モジュールなし。自分で一から組み立てる",
			"一定レートで上向きに放出し、色と大きさがフェードする",
			"一度だけ球状に飛び散り、重力で落ちながら消える",
			"ゆっくり上昇しながら大きく・薄くなる",
			"トレイルレンダラーで移動の軌跡を描く（Motion Dummyで確認）"
		};
		ImGui::SetNextItemWidth(220.0f);
		ImGui::Combo("Template", &addEmitterTemplate_, templates, IM_ARRAYSIZE(templates));
		ImGui::TextDisabled("%s", templateDescriptions[(std::clamp)(addEmitterTemplate_, 0, IM_ARRAYSIZE(templates) - 1)]);

		if (ImGui::Button("Create") || submitted)
		{
			std::string name = emitterNameBuffer_;
			if (name.empty()) name = "Emitter" + std::to_string(currentEffect_->GetEmitterCount() + 1);

			auto emitter = std::make_unique<ParticleEmitter>();
			emitter->Initialize(name);

			switch (addEmitterTemplate_)
			{
			case 1: // Basic
			{
				auto rate = std::make_unique<SpawnRateModule>(); rate->SetRate(20.0f); emitter->AddModule(std::move(rate));
				auto lifetime = std::make_unique<InitialLifetimeModule>(); lifetime->SetLifetimeRange(1.0f, 1.5f); emitter->AddModule(std::move(lifetime));
				auto velocity = std::make_unique<InitialVelocityModule>(); velocity->SetVelocityRange({ -0.5f, 1.5f, -0.5f }, { 0.5f, 3.0f, 0.5f }); emitter->AddModule(std::move(velocity));
				auto color = std::make_unique<ColorFadeModule>(); color->SetUseInitialColor(false); color->SetColors({ 1, 1, 1, 1 }, { 1, 1, 1, 0 }); emitter->AddModule(std::move(color));
				auto scale = std::make_unique<ScaleOverLifetimeModule>(); scale->SetScales({ 0.5f, 0.5f, 0.5f }, { 0.1f, 0.1f, 0.1f }); emitter->AddModule(std::move(scale));
				break;
			}
			case 2: // Burst
			{
				auto burst = std::make_unique<SpawnBurstModule>(); burst->SetCount(60); burst->SetLoops(1); emitter->AddModule(std::move(burst));
				auto shape = std::make_unique<SpawnShapeModule>(); shape->SetShapeType(SpawnShapeType::Sphere); shape->SetOuterRadius(0.2f); emitter->AddModule(std::move(shape));
				auto lifetime = std::make_unique<InitialLifetimeModule>(); lifetime->SetLifetimeRange(0.6f, 1.2f); emitter->AddModule(std::move(lifetime));
				auto radial = std::make_unique<RadialVelocityModule>(); radial->SetSpeedRange(3.0f, 6.0f); emitter->AddModule(std::move(radial));
				auto gravity = std::make_unique<GravityModule>(); gravity->SetGravityRange({ 0, -9.8f, 0 }, { 0, -9.8f, 0 }); emitter->AddModule(std::move(gravity));
				auto drag = std::make_unique<DragModule>(); drag->SetDragRange(1.0f, 1.5f); emitter->AddModule(std::move(drag));
				auto alpha = std::make_unique<AlphaFadeModule>(); alpha->SetStartAlpha(1.0f); alpha->SetEndAlpha(0.0f); emitter->AddModule(std::move(alpha));
				emitter->SetLoopBehavior(LoopBehavior::Once);
				break;
			}
			case 3: // Smoke
			{
				auto rate = std::make_unique<SpawnRateModule>(); rate->SetRate(8.0f); emitter->AddModule(std::move(rate));
				auto shape = std::make_unique<SpawnShapeModule>(); shape->SetShapeType(SpawnShapeType::Circle); shape->SetOuterRadius(0.3f); emitter->AddModule(std::move(shape));
				auto lifetime = std::make_unique<InitialLifetimeModule>(); lifetime->SetLifetimeRange(2.0f, 3.0f); emitter->AddModule(std::move(lifetime));
				auto velocity = std::make_unique<InitialVelocityModule>(); velocity->SetVelocityRange({ -0.1f, 0.6f, -0.1f }, { 0.1f, 1.0f, 0.1f }); emitter->AddModule(std::move(velocity));
				auto noise = std::make_unique<NoiseModule>(); noise->SetStrength(0.3f); emitter->AddModule(std::move(noise));
				auto color = std::make_unique<ColorFadeModule>(); color->SetUseInitialColor(false); color->SetColors({ 0.6f, 0.6f, 0.6f, 0.6f }, { 0.4f, 0.4f, 0.4f, 0.0f }); emitter->AddModule(std::move(color));
				auto scale = std::make_unique<ScaleOverLifetimeModule>(); scale->SetScales({ 0.4f, 0.4f, 0.4f }, { 1.5f, 1.5f, 1.5f }); emitter->AddModule(std::move(scale));
				break;
			}
			case 4: // Trail
			{
				auto rate = std::make_unique<SpawnRateModule>(); rate->SetRate(60.0f); emitter->AddModule(std::move(rate));
				auto lifetime = std::make_unique<InitialLifetimeModule>(); lifetime->SetLifetimeRange(0.5f, 0.5f); emitter->AddModule(std::move(lifetime));
				auto alpha = std::make_unique<AlphaFadeModule>(); alpha->SetStartAlpha(1.0f); alpha->SetEndAlpha(0.0f); emitter->AddModule(std::move(alpha));
				emitter->SetSpawnOnlyWhenMoving(true);
				break;
			}
			default:
				break;
			}

			if (addEmitterTemplate_ == 4)
			{
				auto renderer = std::make_unique<TrailRenderer>();
				renderer->Initialize("./Resources/uvChecker.png");
				emitter->SetRenderer(std::move(renderer));
			}
			else
			{
				auto renderer = std::make_unique<SpriteRenderer>();
				renderer->Initialize("./Resources/uvChecker.png");
				emitter->SetRenderer(std::move(renderer));
			}

			currentEffect_->AddEmitter(std::move(emitter));
			if (!currentEffect_->IsPlaying()) currentEffect_->Play();
			SelectEmitter(currentEffect_->GetEmitterCount() - 1);
			MarkDirty();

			showAddEmitterDialog_ = false;
			emitterNameBuffer_[0] = '\0';
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { showAddEmitterDialog_ = false; ImGui::CloseCurrentPopup(); }

		ImGui::EndPopup();
	}
}

void ParticleEditor::AddModuleDialog(ParticleEmitter* emitter)
{
	if (!emitter) return;

	ImGui::OpenPopup("Add Module");
	ImGui::SetNextWindowSize(ImVec2(520.0f, 460.0f), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Add Module", &showAddModuleDialog_))
	{
		if (ImGui::IsWindowAppearing())
		{
			addModuleFilterBuffer_[0] = '\0';
			ImGui::SetKeyboardFocusHere();
		}
		ImGui::SetNextItemWidth(-200.0f);
		ImGui::InputTextWithHint("##ModuleFilter", "Search modules...", addModuleFilterBuffer_, sizeof(addModuleFilterBuffer_));
		ImGui::SameLine();
		ImGui::RadioButton("All", &addModulePhaseFilter_, 0); ImGui::SameLine();
		ImGui::RadioButton("Spawn", &addModulePhaseFilter_, 1); ImGui::SameLine();
		ImGui::RadioButton("Update", &addModulePhaseFilter_, 2);

		const bool gpuEmitter = emitter->GetSimulationMode() == SimulationMode::GPU;
		const ModuleDescriptor* selected = nullptr;
		const ModuleDescriptor* toAdd = nullptr;
		const ModuleDescriptor* firstVisible = nullptr;

		ImGui::BeginChild("##ModuleList", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 3.2f), ImGuiChildFlags_Borders);
		for (const char* category : kModuleCategories)
		{
			std::vector<const ModuleDescriptor*> entries;
			for (const auto& descriptor : ModuleDescriptorRegistry::GetInstance().GetDescriptors())
			{
				if (!descriptor.factory) continue;
				if (addModulePhaseFilter_ == 1 && descriptor.phase != ModulePhase::Spawn) continue;
				if (addModulePhaseFilter_ == 2 && descriptor.phase != ModulePhase::Update) continue;
				const ModuleInfo info = GetModuleInfo(descriptor.id);
				if (std::string(info.category) != category) continue;
				if (!ContainsIgnoreCase(descriptor.displayName, addModuleFilterBuffer_) &&
					!ContainsIgnoreCase(descriptor.id, addModuleFilterBuffer_) &&
					!ContainsIgnoreCase(info.description, addModuleFilterBuffer_)) continue;
				entries.push_back(&descriptor);
			}
			if (entries.empty()) continue;

			ImGui::SeparatorText(category);
			for (const auto* descriptor : entries)
			{
				if (!firstVisible) firstVisible = descriptor;
				const bool isSelected = addModuleSelection_ == descriptor->id;
				if (isSelected) selected = descriptor;
				const bool alreadyAdded = emitter->GetModuleByName(descriptor->id) != nullptr;

				ImGui::PushID(descriptor->id.c_str());
				ImGui::TextColored(descriptor->phase == ModulePhase::Spawn ? kSpawnColor : kUpdateColor,
					descriptor->phase == ModulePhase::Spawn ? "S" : "U");
				ImGui::SameLine();
				const std::string label = descriptor->displayName + (alreadyAdded ? "  (added)" : "");
				if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap))
				{
					addModuleSelection_ = descriptor->id;
					selected = descriptor;
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) toAdd = descriptor;
				}
				const char* tag = descriptor->pureGpuSupported ? "GPU" : "CPU";
				ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(tag).x);
				if (descriptor->pureGpuSupported) ImGui::TextColored(kSpawnColor, "%s", tag);
				else if (gpuEmitter) ImGui::TextColored(kWarningColor, "%s", tag);
				else ImGui::TextDisabled("%s", tag);
				ImGui::PopID();
			}
		}
		if (!firstVisible) ImGui::TextDisabled("(No matching modules)");
		ImGui::EndChild();

		// 検索結果が1件に絞られた時や未選択時は先頭を対象にする（Enterで追加できるように）
		if (!selected) selected = firstVisible;

		if (selected)
		{
			const ModuleInfo info = GetModuleInfo(selected->id);
			ImGui::TextWrapped("%s: %s", selected->displayName.c_str(), info.description);
			ImGui::TextDisabled("%s | v%u | %s", selected->phase == ModulePhase::Spawn ? "Particle Spawn" : "Particle Update",
				selected->version, selected->pureGpuSupported ? "Pure GPU" : "CPU only");
			if (gpuEmitter && !selected->pureGpuSupported)
			{
				ImGui::SameLine();
				ImGui::TextColored(kWarningColor, "(GPUエミッターはCPU実行に切り替わります)");
			}
		}
		else
		{
			ImGui::NewLine();
			ImGui::NewLine();
		}

		ImGui::BeginDisabled(!selected);
		if (ImGui::Button("Add") || (selected && ImGui::IsKeyPressed(ImGuiKey_Enter))) toAdd = selected;
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { showAddModuleDialog_ = false; ImGui::CloseCurrentPopup(); }

		if (toAdd)
		{
			auto module = toAdd->factory();
			IModule* raw = module.get();
			emitter->AddModule(std::move(module));
			selectedModule_ = raw;
			inspectorTarget_ = InspectorTarget::Module;
			MarkDirty();
			showAddModuleDialog_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void ParticleEditor::SaveAsDialog()
{
	if (ImGui::IsPopupOpen("Save Effect As") == false)
	{
		strncpy_s(saveAsBuffer_, effectNameBuffer_[0] != '\0' ? effectNameBuffer_ : "NewEffect", _TRUNCATE);
	}
	ImGui::OpenPopup("Save Effect As");

	if (ImGui::BeginPopupModal("Save Effect As", &showSaveAsDialog_, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const std::filesystem::path directory = GetParticleDirectory();
		ImGui::TextDisabled("%s", directory.generic_string().c_str());
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(260.0f);
		const bool submitted = ImGui::InputText("##SaveAsName", saveAsBuffer_, sizeof(saveAsBuffer_), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		ImGui::TextUnformatted(".json");
		ImGui::TextDisabled("サブフォルダは / で区切って指定できます");

		// ファイル名の検証（空・親ディレクトリ参照・使用不可文字を拒否）
		const std::string fileName = saveAsBuffer_;
		const bool invalid = fileName.empty() || fileName.find("..") != std::string::npos ||
			fileName.find_first_of(":*?\"<>|\\") != std::string::npos || fileName.front() == '/';
		const std::filesystem::path target = directory / (fileName + ".json");
		std::error_code existsError;
		const bool exists = !invalid && std::filesystem::exists(target, existsError);
		if (invalid) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "無効なファイル名です");
		else if (exists) ImGui::TextColored(kWarningColor, "既存のファイルを上書きします");

		ImGui::BeginDisabled(invalid);
		if (ImGui::Button("Save") || (submitted && !invalid))
		{
			std::error_code directoryError;
			std::filesystem::create_directories(target.parent_path(), directoryError);
			if (directoryError)
			{
				operationSucceeded_ = false;
				operationStatus_ = "Save failed: cannot create directory " + target.parent_path().string();
			}
			else
			{
				SaveEffect(target.string());
			}
			showSaveAsDialog_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { showSaveAsDialog_ = false; ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
}

void ParticleEditor::DiscardConfirmDialog()
{
	if (openDiscardConfirm_)
	{
		ImGui::OpenPopup("Unsaved Changes");
		openDiscardConfirm_ = false;
	}

	if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("\"%s\" には未保存の変更があります。", effectNameBuffer_);
		ImGui::Text("変更を破棄して続けますか？");
		ImGui::Spacing();

		std::function<void()> action;
		if (!effectPath_.empty() && ImGui::Button("Save and Continue"))
		{
			if (SaveEffect(effectPath_)) action = std::move(pendingDiscardAction_);
			ImGui::CloseCurrentPopup();
		}
		if (!effectPath_.empty()) ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button("Discard"))
		{
			action = std::move(pendingDiscardAction_);
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			pendingDiscardAction_ = nullptr;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();

		if (action) action();
	}
}
#endif

void ParticleEditor::RunWithDiscardCheck(std::function<void()> action)
{
	if (!isDirty_)
	{
		action();
		return;
	}
	pendingDiscardAction_ = std::move(action);
	openDiscardConfirm_ = true;
}

void ParticleEditor::DuplicateEmitter(size_t index)
{
	if (!currentEffect_) return;
	const auto* source = currentEffect_->GetEmitter(index);
	if (!source) return;

	auto copy = ParticleEffectSerializer::CloneEmitter(*source);
	if (!copy)
	{
		operationSucceeded_ = false;
		operationStatus_ = "Duplicate failed: " + source->GetName();
		return;
	}
	copy->SetName(source->GetName() + "_Copy");
	// GPUイベントは前方のエミッターしか参照できないため、末尾への複製でも参照先はそのまま有効
	currentEffect_->AddEmitter(std::move(copy));
	if (currentEffect_->IsPlaying()) currentEffect_->GetEmitter(currentEffect_->GetEmitterCount() - 1)->Restart();
	SelectEmitter(currentEffect_->GetEmitterCount() - 1);
	MarkDirty();
}

void ParticleEditor::SetEmitterMuted(ParticleEmitter* emitter, bool muted)
{
	if (!emitter) return;
	emitter->SetEnabled(!muted);
	if (muted)
	{
		// ミュート時は残っているパーティクルも消して、他エミッターだけを見やすくする
		emitter->Reset();
	}
	else
	{
		emitter->Restart();
	}
}

void ParticleEditor::SoloEmitter(size_t index)
{
	if (!currentEffect_) return;
	// 既にこのエミッターだけが有効ならソロ解除（全員ミュート解除）
	bool alreadySolo = true;
	for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
	{
		const auto* emitter = currentEffect_->GetEmitter(i);
		if (emitter && emitter->IsEnabled() != (i == index)) { alreadySolo = false; break; }
	}
	for (size_t i = 0; i < currentEffect_->GetEmitterCount(); ++i)
	{
		auto* emitter = currentEffect_->GetEmitter(i);
		const bool muted = alreadySolo ? false : (i != index);
		if (emitter && emitter->IsEnabled() == muted) SetEmitterMuted(emitter, muted);
	}
}

ParticleEmitter* ParticleEditor::GetSelectedEmitter() const
{
	if (!currentEffect_ || selectedEmitterIndex_ < 0) return nullptr;
	return currentEffect_->GetEmitter(static_cast<size_t>(selectedEmitterIndex_));
}

IModule* ParticleEditor::GetSelectedModule(ParticleEmitter* emitter) const
{
	if (!emitter || !selectedModule_) return nullptr;
	for (size_t i = 0; i < emitter->GetModuleCount(); ++i)
	{
		if (emitter->GetModule(i) == selectedModule_) return selectedModule_;
	}
	return nullptr;
}
} // namespace KCE
