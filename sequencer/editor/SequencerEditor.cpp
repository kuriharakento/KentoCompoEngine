#include "sequencer/editor/SequencerEditor.h"

#include "base/Camera.h"
#include "manager/scene/CameraManager.h"

namespace KCE
{
std::unique_ptr<SequencerEditor> SequencerEditor::instance_ = nullptr;

SequencerEditor* SequencerEditor::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<SequencerEditor>();
	}
	return instance_.get();
}

bool SequencerEditor::HasInstance()
{
	return instance_ != nullptr;
}
} // namespace KCE

#ifdef USE_IMGUI

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ImGuizmo/ImGuizmo.h"
#include "audio/Audio.h"
#include "base/Logger.h"
#include "editor/EditorContext.h"
#include "editor/SceneViewContext.h"
#include "editor/SelectionContext.h"
#include "editor/command/CommandHistory.h"
#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"
#include "manager/editor/DebugUIManager.h"
#include "manager/scene/LightManager.h"
#include "sequencer/core/TrackFactory.h"
#include "sequencer/editor/SequencerCommands.h"
#include "sequencer/track/CameraTrack.h"
#include "sequencer/track/ComponentTrack.h"
#include "sequencer/track/LightTrack.h"

namespace KCE
{
namespace
{
/** @brief シーケンスカメラに割り当てる役の名前 */
const char* const kCameraRole = "MainCam";
/** @brief 編集用の自由移動カメラの名前 */
const char* const kEditorCameraName = "SequencerEditorCamera";
/** @brief シーケンスが駆動するカメラの名前 */
const char* const kSequenceCameraName = "SequencerCamera";

class PreviewObjectBindingCommand : public ICommand
{
public:
	PreviewObjectBindingCommand(std::unordered_map<std::string, Guid>* bindings, std::string role, Guid after)
		: bindings_(bindings), role_(std::move(role)), after_(after)
	{
		const auto it = bindings_->find(role_);
		if (it != bindings_->end())
		{
			before_ = it->second;
			hadBefore_ = true;
		}
	}

	void Execute() override { (*bindings_)[role_] = after_; }
	void Undo() override
	{
		if (hadBefore_) { (*bindings_)[role_] = before_; }
		else { bindings_->erase(role_); }
	}
	std::string GetName() const override { return "Bind Preview Object"; }

private:
	// SequencerEditor が所有し、登録解除時に履歴も破棄される。
	std::unordered_map<std::string, Guid>* bindings_ = nullptr;
	std::string role_;
	Guid before_{};
	Guid after_{};
	bool hadBefore_ = false;
};

/** @brief キーのマーカーの半径（ピクセル） */
constexpr float kKeyMarkerRadius = 5.0f;
/** @brief キーを掴めるとみなす距離（ピクセル） */
constexpr float kKeyGrabRadius = 7.0f;
/** @brief 波形を描き始めるルーラー内のY位置 */
constexpr float kWaveformTop = 28.0f;
/** @brief タップをやり直したとみなす間隔（秒） */
constexpr double kTapResetSeconds = 2.0;
/** @brief 拍の番号を出すのに要る拍どうしの間隔（ピクセル）。これより詰まると文字が重なる */
constexpr float kBeatLabelMinSpacing = 28.0f;
/** @brief 拍の番号を書くルーラー内のY位置 */
constexpr float kBeatLabelTop = 15.0f;
/** @brief 拍の線から番号までのすき間（ピクセル） */
constexpr float kBeatLabelPadding = 2.0f;
/** @brief 波形の下にあけるすき間（ピクセル） */
constexpr float kWaveformBottomMargin = 2.0f;
/** @brief 範囲選択とみなすのに要るドラッグ量（ピクセル）。これより小さければただのクリック */
constexpr float kBoxSelectMinDrag = 4.0f;
/** @brief 貼り付けたキーを探し直すときの時刻の許容差（秒） */
constexpr float kPastedKeyTolerance = 0.001f;
/** @brief ベジェのグラフの1辺（ピクセル） */
constexpr float kBezierGraphSize = 160.0f;
/** @brief ベジェのグラフに映す値の範囲。行き過ぎる（Back 系の）制御点も掴めるよう 0〜1 より広く取る */
constexpr float kBezierGraphMinValue = -0.5f;
constexpr float kBezierGraphMaxValue = 1.5f;
/** @brief ベジェの制御点の半径（ピクセル） */
constexpr float kBezierHandleRadius = 5.0f;
/** @brief ベジェの制御点を掴めるとみなす距離（ピクセル） */
constexpr float kBezierHandleGrabRadius = 9.0f;

/**
 * @brief トラックがタイムライン上で占める行数
 * @details チャンネル1本につき1行。チャンネルを持たないトラックも1行は確保する。
 */
size_t GetRowCount(ITrack* track)
{
	return (std::max)(track->GetChannelCount(), static_cast<size_t>(1));
}

/**
 * @brief 選択中のキーが指すチャンネルを取得する
 * @return チャンネル。選択がキーでない、または範囲外なら nullptr
 */
ICurveChannel* GetSelectedChannel(Sequence& sequence, const SelectionItem& item)
{
	if (item.kind != SelectionKind::SequenceKey || item.trackIndex < 0 || item.channelIndex < 0)
	{
		return nullptr;
	}
	ITrack* track = sequence.GetTrack(static_cast<size_t>(item.trackIndex));
	if (!track)
	{
		return nullptr;
	}
	ICurveChannel* channel = track->GetChannel(static_cast<size_t>(item.channelIndex));
	if (!channel || item.keyIndex < 0 || static_cast<size_t>(item.keyIndex) >= channel->GetKeyCount())
	{
		return nullptr;
	}
	return channel;
}

/**
 * @brief 現在の主選択を取得する
 */
const SelectionItem& GetPrimarySelection()
{
	return SelectionContext::GetInstance()->GetPrimary();
}

/**
 * @brief 選択の中から、今も存在するキーだけを集める
 * @details 操作のたびに1回呼ぶだけなので、ここの確保は許す
 */
std::vector<SelectionItem> CollectSelectedKeys(Sequence& sequence)
{
	std::vector<SelectionItem> keys;
	for (const SelectionItem& item : SelectionContext::GetInstance()->GetItems())
	{
		if (GetSelectedChannel(sequence, item))
		{
			keys.push_back(item);
		}
	}
	return keys;
}

/**
 * @brief 秒数を mm:ss.mmm 形式に整形する
 */
void FormatTime(char* buffer, size_t bufferSize, float seconds)
{
	const int minutes = static_cast<int>(seconds) / 60;
	const float rest = seconds - static_cast<float>(minutes * 60);
	std::snprintf(buffer, bufferSize, "%02d:%06.3f", minutes, rest);
}

/**
 * @brief トラック種別から、役に割り当てる実体の型を決める
 * @return 役を持たないトラック（PostProcess）なら偽
 */
bool GetBindingTypeForTrack(const ITrack& track, BindingType& outType)
{
	switch (track.GetType())
	{
	case TrackType::Camera:    outType = BindingType::Camera; return true;
	case TrackType::Transform: outType = BindingType::GameObject; return true;
	case TrackType::Light:     outType = BindingType::Light; return true;
	case TrackType::Screen:    outType = BindingType::GameObject; return true;
	case TrackType::Component: outType = BindingType::GameObject; return true;
	default:                   return false;
	}
}

bool HasRequiredBinding(const ITrack& track, const BindingContext& context)
{
	BindingType type;
	if (!GetBindingTypeForTrack(track, type)) { return true; }
	if (const auto* lightTrack = dynamic_cast<const LightTrack*>(&track))
	{
		if (lightTrack->GetKind() == LightTrackKind::Directional) { return true; }
	}
	if (track.GetBindingRole().empty()) { return false; }
	switch (type)
	{
	case BindingType::GameObject: return context.GetGameObject(track.GetBindingRole()) != nullptr;
	case BindingType::Camera:     return context.GetCamera(track.GetBindingRole()) != nullptr;
	case BindingType::Light:      return !context.GetLightName(track.GetBindingRole()).empty();
	default:                      return true;
	}
}

/**
 * @brief トラックへの編集を1コマンドとして履歴に積む
 * @details 編集前にスナップショットを取り、edit を実行し、変化があれば積む。
 * @return 変化があれば真
 */
template<class EditFunc>
bool ExecuteTrackEdit(Sequence& sequence, size_t trackIndex, const char* name, EditFunc edit)
{
	auto command = std::make_unique<TrackEditCommand>(&sequence, trackIndex, name);
	edit();
	command->CaptureAfter();
	if (!command->HasChanged())
	{
		return false;
	}
	CommandHistory::GetInstance()->Execute(std::move(command));
	return true;
}

/** @brief メタ情報の変更を履歴へ積む */
void ExecuteMetaEdit(Sequence& sequence, const SequenceMeta& after, const char* name, uint32_t editId)
{
	const SequenceMeta before = sequence.GetMeta();
	if (before.bpm == after.bpm && before.offset == after.offset && before.audioClip == after.audioClip)
	{
		return;
	}
	CommandHistory::GetInstance()->Execute(
		std::make_unique<SequenceMetaCommand>(&sequence, before, after, name, editId));
}
} // namespace

void SequencerEditor::Initialize(CameraManager* cameraManager, LightManager* lightManager, PostProcessManager* postProcessManager)
{
	if (initialized_)
	{
		return;
	}

	cameraManager_ = cameraManager;
	lightManager_ = lightManager;
	postProcessManager_ = postProcessManager;
	sequenceCameraName_ = kSequenceCameraName;
	editorCameraName_ = kEditorCameraName;

	if (cameraManager_)
	{
		// シーケンスが駆動するカメラと、それを外から眺めるための編集用カメラを用意する。
		// 1台しか無いとギズモを自分自身の視点から動かすことになり、まともに置けない。
		cameraManager_->AddCamera(sequenceCameraName_);
		cameraManager_->AddCamera(editorCameraName_);

		if (Camera* editorCamera = cameraManager_->GetCamera(editorCameraName_))
		{
			editorCamera->SetTranslate({ 0.0f, 4.0f, -14.0f });
			editorCamera->SetRotate({ 0.0f, 0.0f, 0.0f });
		}

		SetSequenceCamera(cameraManager_->GetCamera(sequenceCameraName_));
	}

	// ライト・ポストプロセスは役を持たない共有システムとして渡す
	player_.GetBindingContext().SetLightManager(lightManager_);
	player_.GetBindingContext().SetPostProcessManager(postProcessManager_);

	// シーケンスは必ず MainCam の役を要求する
	BindingDefinition binding;
	binding.role = kCameraRole;
	binding.type = BindingType::Camera;
	binding.description = "シーケンスが動かすカメラ";
	sequence_.AddBinding(binding);

	player_.SetSequence(&sequence_);

	// エディタでの再生中はイベントの発火を画面とログに出して確認できるようにする
	player_.SetEventCallback([this](const std::string& eventName)
	{
		statusMessage_ = "Event: " + eventName;
		Logger::Log("Sequencer Event: " + eventName + "\n");
	});

	DebugUIManager* debugUI = DebugUIManager::GetInstance();
	debugUI->RegisterWindow(this, "Sequencer", [this]() { DrawTimelineWindow(); }, EditorDock::Bottom);
	debugUI->RegisterInspector(this, SelectionKind::SequenceTrack, [this](const SelectionItem&) { DrawInspectorWindow(); });
	debugUI->RegisterInspector(this, SelectionKind::SequenceKey, [this](const SelectionItem&) { DrawInspectorWindow(); });
	debugUI->RegisterInspector(this, SelectionKind::GameObject,
		[this](const SelectionItem& item) { DrawGameObjectSequencerInspector(item); });
	debugUI->RegisterSceneOverlay(this, [this]() { DrawSceneOverlay(); });

	initialized_ = true;
}

void SequencerEditor::Finalize()
{
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}

	player_.SetSequence(nullptr);
	sequence_.Clear();
	previewObjectBindings_.clear();
	previewLightBindings_.clear();
	cameraManager_ = nullptr;
	lightManager_ = nullptr;
	postProcessManager_ = nullptr;
	initialized_ = false;
	instance_.reset();
}

void SequencerEditor::SetSequenceCamera(Camera* camera)
{
	player_.GetBindingContext().BindCamera(kCameraRole, camera);
}

void SequencerEditor::SetAtmosphere(FogRenderer* fogRenderer, BeamRenderer* beamRenderer)
{
	player_.GetBindingContext().SetFogRenderer(fogRenderer);
	player_.GetBindingContext().SetBeamRenderer(beamRenderer);
}

void SequencerEditor::SetTextOverlay(TextOverlay* textOverlay)
{
	player_.GetBindingContext().SetTextOverlay(textOverlay);
}

Camera* SequencerEditor::GetSequenceCamera() const
{
	return player_.GetBindingContext().GetCamera(kCameraRole);
}

void SequencerEditor::ApplyPreviewBindings()
{
	BindingContext& ctx = player_.GetBindingContext();

	// GameObject は GUID から毎フレーム引き直す。破棄されていれば割り当ては外れる
	for (const auto& [role, guid] : previewObjectBindings_)
	{
		GameObject* object = GameObjectManager::HasInstance() ? GameObjectManager::GetInstance()->FindByGuid(guid) : nullptr;
		ctx.BindGameObject(role, object);
	}

	for (const auto& [role, lightName] : previewLightBindings_)
	{
		ctx.BindLight(role, lightName);
	}
}

std::string SequencerEditor::MakeUniqueObjectRole(const GameObject& object) const
{
	const std::string base = object.GetName().empty() ? "GameObject" : object.GetName();
	std::string role = base;
	for (uint32_t suffix = 2;; ++suffix)
	{
		const auto it = previewObjectBindings_.find(role);
		if (it == previewObjectBindings_.end() || it->second == object.GetGuid())
		{
			return role;
		}
		role = base + " " + std::to_string(suffix);
	}
}

int SequencerEditor::AddGameObjectTrack(const std::string& typeName, GameObject& object)
{
	TrackPtr track = TrackFactory::Create(typeName);
	if (!track)
	{
		statusMessage_ = "未知のトラック種別です: " + typeName;
		return -1;
	}

	const std::string role = MakeUniqueObjectRole(object);
	track->SetBindingRole(role);
	track->SetName(object.GetName() + " " + typeName);

	auto structure = std::make_unique<SequenceStructureCommand>(&sequence_, "Add " + typeName + " Track");
	BindingDefinition definition;
	definition.role = role;
	definition.type = BindingType::GameObject;
	sequence_.AddBinding(definition);
	sequence_.AddTrack(std::move(track));
	structure->CaptureAfter();

	CommandHistory* history = CommandHistory::GetInstance();
	history->BeginTransaction("Add GameObject Track");
	if (structure->HasChanged())
	{
		history->Execute(std::move(structure));
	}
	history->Execute(std::make_unique<PreviewObjectBindingCommand>(
		&previewObjectBindings_, role, object.GetGuid()));
	history->EndTransaction();

	ApplyPreviewBindings();
	const int trackIndex = static_cast<int>(sequence_.GetTrackCount()) - 1;
	SelectionItem selected;
	selected.kind = SelectionKind::SequenceTrack;
	selected.trackIndex = trackIndex;
	SelectionContext::GetInstance()->Select(selected);
	pendingScrollToTrack_ = selected.trackIndex;
	return trackIndex;
}

int SequencerEditor::FindTargetCameraTrackIndex() const
{
	const int selected = GetSelectedTrackIndex();
	if (selected >= 0 && dynamic_cast<CameraTrack*>(sequence_.GetTrack(static_cast<size_t>(selected))))
	{
		return selected;
	}
	for (size_t index = 0; index < sequence_.GetTrackCount(); ++index)
	{
		if (dynamic_cast<CameraTrack*>(sequence_.GetTrack(index)))
		{
			return static_cast<int>(index);
		}
	}
	return -1;
}

void SequencerEditor::DrawGameObjectSequencerInspector(const SelectionItem& item)
{
	GameObject* object = GameObjectManager::HasInstance()
		? GameObjectManager::GetInstance()->FindByGuid(item.objectGuid)
		: nullptr;
	if (!object)
	{
		return;
	}

	ImGui::SeparatorText("Sequencer");
	if (ImGui::Button("Transform トラックを作る"))
	{
		AddGameObjectTrack("Transform", *object);
	}
	if (ImGui::Button("Component トラックを作る"))
	{
		AddGameObjectTrack("Component", *object);
	}

	const int cameraTrackIndex = FindTargetCameraTrackIndex();
	const bool hasCameraTrack = cameraTrackIndex >= 0;
	if (!hasCameraTrack) { ImGui::BeginDisabled(); }
	auto setAimRole = [&](bool useRoleB)
	{
		CameraTrack* cameraTrack = dynamic_cast<CameraTrack*>(sequence_.GetTrack(static_cast<size_t>(cameraTrackIndex)));
		const std::string role = MakeUniqueObjectRole(*object);
		auto edit = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(cameraTrackIndex), "Set Camera Aim Role");
		if (useRoleB) { cameraTrack->SetAimRoleB(role); }
		else { cameraTrack->SetAimRoleA(role); }
		edit->CaptureAfter();

		CommandHistory* history = CommandHistory::GetInstance();
		history->BeginTransaction("Set Camera Aim Target");
		if (edit->HasChanged()) { history->Execute(std::move(edit)); }
		history->Execute(std::make_unique<PreviewObjectBindingCommand>(
			&previewObjectBindings_, role, object->GetGuid()));
		history->EndTransaction();
		ApplyPreviewBindings();

		SelectionItem selected;
		selected.kind = SelectionKind::SequenceTrack;
		selected.trackIndex = cameraTrackIndex;
		SelectionContext::GetInstance()->Select(selected);
	};
	if (ImGui::Button("カメラの注目点 A にする")) { setAimRole(false); }
	if (ImGui::Button("カメラの注目点 B にする")) { setAimRole(true); }
	if (!hasCameraTrack)
	{
		ImGui::EndDisabled();
		ImGui::TextDisabled("カメラトラックがありません");
	}
}

void SequencerEditor::Update()
{
	if (!initialized_)
	{
		return;
	}

	ApplyPreviewBindings();
	HandleShortcuts();
	UpdateEditorCameraFly();
	player_.Update();
}

int SequencerEditor::GetSelectedTrackIndex() const
{
	const SelectionItem& selected = GetPrimarySelection();
	if ((selected.kind == SelectionKind::SequenceKey || selected.kind == SelectionKind::SequenceTrack) &&
		selected.trackIndex >= 0 && static_cast<size_t>(selected.trackIndex) < sequence_.GetTrackCount())
	{
		return selected.trackIndex;
	}
	return -1;
}

///=============================================================================
///						ツールバー
///=============================================================================

void SequencerEditor::DrawToolbar()
{
	EditorContext* editorContext = EditorContext::GetInstance();

	// --- モード切り替え ---
	const bool isEditMode = editorContext->IsEditMode();
	if (ImGui::Button(isEditMode ? "Mode: Edit" : "Mode: Play"))
	{
		editorContext->ToggleMode();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("編集モードではゲーム時間が止まり、\n絵はシーケンサの時刻だけで決まります");
	}

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	// --- 再生操作 ---
	if (ImGui::Button("|<"))
	{
		player_.Seek(0.0f);
	}
	ImGui::SameLine();

	const bool isPlaying = player_.IsPlaying();
	if (ImGui::Button(isPlaying ? "Pause" : "Play"))
	{
		if (isPlaying) { player_.Pause(); }
		else { player_.Resume(); }
	}
	ImGui::SameLine();

	if (ImGui::Button("Stop"))
	{
		player_.Stop();
	}
	ImGui::SameLine();

	if (ImGui::Button("Skip"))
	{
		// 純関数契約が守られていれば、これだけで最終状態になる
		player_.SkipToEnd();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("末尾へ飛ばします（カットシーンのスキップ相当）");
	}

	ImGui::SameLine();
	bool loop = player_.IsLoop();
	if (ImGui::Checkbox("Loop", &loop))
	{
		player_.SetLoop(loop);
	}

	// --- 時刻表示 ---
	ImGui::SameLine();
	char currentBuffer[32];
	char durationBuffer[32];
	FormatTime(currentBuffer, sizeof(currentBuffer), player_.GetTime());
	FormatTime(durationBuffer, sizeof(durationBuffer), player_.GetDuration());
	ImGui::Text("%s / %s", currentBuffer, durationBuffer);

	if (player_.IsAudioDriven())
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[audio-sync]");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("時刻はオーディオの再生位置を権威としています");
		}
	}

	// --- 2行目: 編集操作 ---
	size_t typeCount = 0;
	const char* const* typeNames = TrackFactory::GetCreatableTypeNames(typeCount);
	ImGui::SetNextItemWidth(120.0f);
	ImGui::Combo("##TrackType", &addTrackTypeIndex_, typeNames, static_cast<int>(typeCount));
	ImGui::SameLine();
	if (ImGui::Button("Add Track"))
	{
		if (addTrackTypeIndex_ >= 0 && static_cast<size_t>(addTrackTypeIndex_) < typeCount)
		{
			AddTrack(typeNames[addTrackTypeIndex_]);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Key (K)"))
	{
		AddKeyAtCurrentTime();
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete Key (Del)"))
	{
		DeleteSelectedKey();
	}

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	CommandHistory* history = CommandHistory::GetInstance();
	ImGui::BeginDisabled(!history->CanUndo());
	if (ImGui::Button("Undo"))
	{
		history->Undo();
		player_.EvaluateCurrentTime();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!history->CanRedo());
	if (ImGui::Button("Redo"))
	{
		history->Redo();
		player_.EvaluateCurrentTime();
	}
	ImGui::EndDisabled();

	// --- 3行目: プレビューとギズモ ---
	if (ImGui::Checkbox("Look through sequence camera", &previewThroughSequenceCamera_))
	{
		ApplyActiveCamera();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("オフの間は編集用カメラから眺め、シーケンスカメラをギズモで操作できます");
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::Combo("Gizmo", &gizmoOperation_, "Translate\0Rotate\0Scale\0");
	ImGui::SameLine();
	ImGui::Checkbox("World", &gizmoWorldSpace_);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::DragFloat("Fly Speed", &editorCameraSpeed_, 0.1f, 0.1f, 200.0f, "%.1f");

	// --- 4行目: 保存とスナップ ---
	char pathBuffer[256];
	std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", filePath_.c_str());
	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::InputText("File", pathBuffer, sizeof(pathBuffer)))
	{
		filePath_ = pathBuffer;
	}

	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		if (sequence_.SaveToFile(filePath_))
		{
			statusMessage_ = "保存しました: " + filePath_;
			CommandHistory::GetInstance()->MarkSaved();
		}
		else
		{
			statusMessage_ = "保存に失敗しました: " + filePath_;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		// 読み込みでシーケンスの中身が入れ替わるため、選択と履歴を捨てる。
		// 残すと、消えたトラックを指したままの選択やUndoが残ってしまう。
		player_.Stop();
		SelectionContext::GetInstance()->ClearSelection();
		CommandHistory::GetInstance()->Clear();

		std::string error;
		if (sequence_.LoadFromFile(filePath_, &error))
		{
			statusMessage_ = "読み込みました: " + filePath_;
			player_.EvaluateCurrentTime();
		}
		else
		{
			statusMessage_ = "読み込みに失敗しました: " + error;
		}
	}

	ImGui::SameLine();
	ImGui::Checkbox("Snap", &snapEnabled_);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::DragFloat("Interval", &snapInterval_, 0.01f, 0.01f, 5.0f, "%.2f s");

	if (!statusMessage_.empty())
	{
		if (observedStatusMessage_ != statusMessage_)
		{
			observedStatusMessage_ = statusMessage_;
			statusMessageTime_ = ImGui::GetTime();
		}
		constexpr double kStatusDisplaySeconds = 4.0;
		if (ImGui::GetTime() - statusMessageTime_ <= kStatusDisplaySeconds)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "%s", statusMessage_.c_str());
		}
	}
}

///=============================================================================
///						タイムライン
///=============================================================================

float SequencerEditor::TimeToPixel(float time, float canvasLeft) const
{
	return canvasLeft + (time - view_.scrollTime) * view_.pixelsPerSecond;
}

float SequencerEditor::PixelToTime(float pixelX, float canvasLeft) const
{
	return view_.scrollTime + (pixelX - canvasLeft) / view_.pixelsPerSecond;
}

float SequencerEditor::SnapTime(float time) const
{
	if (!snapEnabled_)
	{
		return time;
	}

	// BPMが設定されていれば拍にスナップする。楽曲演出では秒よりも拍が基準になる。
	const SequenceMeta& meta = sequence_.GetMeta();
	if (meta.bpm > 0.0f)
	{
		const float beatDuration = 60.0f / meta.bpm;
		const float relative = time - meta.offset;
		return meta.offset + std::round(relative / beatDuration) * beatDuration;
	}

	if (snapInterval_ <= 0.0f)
	{
		return time;
	}
	return std::round(time / snapInterval_) * snapInterval_;
}

void SequencerEditor::FrameAllKeys()
{
	float endTime = 0.0f;
	for (const auto& track : sequence_.GetTracks())
	{
		if (track) { endTime = (std::max)(endTime, track->GetEndTime()); }
	}
	constexpr float kMinimumVisibleSeconds = 1.0f;
	constexpr float kFramePadding = 40.0f;
	const float width = (std::max)(ImGui::GetContentRegionAvail().x - view_.headerWidth - kFramePadding, 50.0f);
	view_.scrollTime = 0.0f;
	view_.pixelsPerSecond = std::clamp(width / (std::max)(endTime, kMinimumVisibleSeconds), 5.0f, 2000.0f);
}

size_t SequencerEditor::GetVisibleRowCount(size_t trackIndex) const
{
	ITrack* track = sequence_.GetTrack(trackIndex);
	if (!track) { return 0; }
	const bool collapsed = std::find(collapsedTracks_.begin(), collapsedTracks_.end(), trackIndex) != collapsedTracks_.end();
	return 1 + (collapsed ? 0 : track->GetChannelCount());
}

void SequencerEditor::DrawRuler(const ImVec2& canvasMin, float canvasWidth)
{
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float left = canvasMin.x + view_.headerWidth;
	const float rulerBottom = canvasMin.y + view_.rulerHeight;

	drawList->AddRectFilled(
		ImVec2(canvasMin.x, canvasMin.y),
		ImVec2(canvasMin.x + canvasWidth, rulerBottom),
		IM_COL32(38, 38, 42, 255));

	const float startTime = view_.scrollTime;
	const float endTime = PixelToTime(canvasMin.x + canvasWidth, left);

	// 目盛りの間隔は、画面上で40ピクセル以上空くように選ぶ
	static const float kCandidateSteps[] = { 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 30.0f, 60.0f };
	float step = kCandidateSteps[0];
	for (float candidate : kCandidateSteps)
	{
		step = candidate;
		if (candidate * view_.pixelsPerSecond >= 40.0f)
		{
			break;
		}
	}

	const float firstTick = std::floor(startTime / step) * step;
	for (float t = firstTick; t <= endTime; t += step)
	{
		const float x = TimeToPixel(t, left);
		if (x < left)
		{
			continue;
		}

		drawList->AddLine(ImVec2(x, canvasMin.y + view_.rulerHeight * 0.4f), ImVec2(x, rulerBottom), IM_COL32(120, 120, 130, 255));

		char label[32];
		std::snprintf(label, sizeof(label), "%.2f", t);
		drawList->AddText(ImVec2(x + 2.0f, canvasMin.y + 2.0f), IM_COL32(180, 180, 190, 255), label);
	}

	// ビートグリッド
	const SequenceMeta& meta = sequence_.GetMeta();
	if (meta.bpm > 0.0f)
	{
		const float beatDuration = 60.0f / meta.bpm;
		if (beatDuration * view_.pixelsPerSecond >= 6.0f)
		{
			const int firstBeatIndex = static_cast<int>(std::floor((startTime - meta.offset) / beatDuration));
			for (int beat = firstBeatIndex; ; ++beat)
			{
				const float t = meta.offset + static_cast<float>(beat) * beatDuration;
				if (t > endTime) { break; }

				const float x = TimeToPixel(t, left);
				if (x < left) { continue; }

				// 4拍ごとに濃くして小節の頭が分かるようにする
				const bool isBarStart = beat % 4 == 0;
				drawList->AddLine(
					ImVec2(x, rulerBottom),
					ImVec2(x, canvasMin.y + ImGui::GetContentRegionAvail().y),
					isBarStart ? IM_COL32(90, 90, 110, 160) : IM_COL32(70, 70, 80, 90));

				if (beat >= 0 && beatDuration * view_.pixelsPerSecond >= kBeatLabelMinSpacing)
				{
					char beatLabel[24];
					std::snprintf(beatLabel, sizeof(beatLabel), "%d.%d", beat / 4 + 1, beat % 4 + 1);
					drawList->AddText(ImVec2(x + kBeatLabelPadding, canvasMin.y + kBeatLabelTop), IM_COL32(130, 190, 220, 255), beatLabel);
				}
			}
		}
	}

	// マーカー
	for (const auto& marker : sequence_.GetMarkers())
	{
		const float x = TimeToPixel(marker.time, left);
		if (x < left) { continue; }
		drawList->AddLine(ImVec2(x, canvasMin.y), ImVec2(x, rulerBottom), IM_COL32(240, 200, 90, 255), 2.0f);
		drawList->AddText(ImVec2(x + 3.0f, canvasMin.y + view_.rulerHeight * 0.5f), IM_COL32(240, 200, 90, 255), marker.name.c_str());
	}
}

void SequencerEditor::DrawWaveform(const ImVec2& canvasMin, float canvasWidth)
{
	const SequenceMeta& meta = sequence_.GetMeta();
	if (meta.audioClip.empty())
	{
		return;
	}

	const std::vector<SoundData::WaveformPeak>* peaks = Audio::GetInstance()->GetWaveformPeaks(meta.audioClip);
	if (!peaks || peaks->empty())
	{
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float left = canvasMin.x + view_.headerWidth;
	const float right = canvasMin.x + canvasWidth;
	const float top = canvasMin.y + kWaveformTop;
	const float bottom = canvasMin.y + view_.rulerHeight - kWaveformBottomMargin;
	const float center = (top + bottom) * 0.5f;
	const float amplitude = (bottom - top) * 0.5f;
	const float secondsPerPeak = Audio::GetInstance()->GetWaveformSecondsPerPeak(meta.audioClip);
	if (secondsPerPeak <= 0.0f)
	{
		return;
	}
	const float audioStart = (std::max)(view_.scrollTime + meta.offset, 0.0f);
	const float audioEnd = (std::max)(PixelToTime(right, left) + meta.offset, 0.0f);
	const size_t firstPeak = (std::min)(static_cast<size_t>(audioStart / secondsPerPeak), peaks->size());
	const size_t lastPeak = (std::min)(static_cast<size_t>(std::ceil(audioEnd / secondsPerPeak)) + 1, peaks->size());
	const size_t peaksPerPixel = (std::max)(static_cast<size_t>(std::ceil(1.0f / (secondsPerPeak * view_.pixelsPerSecond))), static_cast<size_t>(1));

	drawList->PushClipRect(ImVec2(left, top), ImVec2(right, bottom), true);
	for (size_t index = firstPeak; index < lastPeak; index += peaksPerPixel)
	{
		const size_t groupEnd = (std::min)(index + peaksPerPixel, lastPeak);
		float minimum = 1.0f;
		float maximum = -1.0f;
		for (size_t peakIndex = index; peakIndex < groupEnd; ++peakIndex)
		{
			minimum = (std::min)(minimum, (*peaks)[peakIndex].minimum);
			maximum = (std::max)(maximum, (*peaks)[peakIndex].maximum);
		}
		const float timelineTime = static_cast<float>(index) * secondsPerPeak - meta.offset;
		const float x = TimeToPixel(timelineTime, left);
		drawList->AddLine(
			ImVec2(x, center - maximum * amplitude),
			ImVec2(x, center - minimum * amplitude),
			IM_COL32(90, 180, 210, 210));
	}
	drawList->PopClipRect();
}

void SequencerEditor::DrawTracks(const ImVec2& canvasMin, const ImVec2& canvasSize)
{
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(
		ImVec2(canvasMin.x, canvasMin.y + view_.rulerHeight),
		ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y), true);
	SelectionContext* selection = SelectionContext::GetInstance();
	const float left = canvasMin.x + view_.headerWidth;
	const int selectedTrack = GetSelectedTrackIndex();

	float rowY = canvasMin.y + view_.rulerHeight - view_.verticalScroll;
	size_t globalRow = 0;

	for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount(); ++trackIndex)
	{
		ITrack* track = sequence_.GetTrack(trackIndex);
		if (!track)
		{
			continue;
		}

		const size_t rowCount = GetVisibleRowCount(trackIndex);
		const bool hasBinding = HasRequiredBinding(*track, player_.GetBindingContext());
		for (size_t row = 0; row < rowCount; ++row, ++globalRow)
		{
			const float rowTop = rowY;
			const float rowBottom = rowY + view_.trackHeight;
			const float rowCenter = (rowTop + rowBottom) * 0.5f;
			ICurveChannel* channel = row > 0 ? track->GetChannel(row - 1) : nullptr;

			// 行の背景。選択中のトラックは明るくし、それ以外は1行おきに明度を変える
			ImU32 background = (globalRow % 2 == 0) ? IM_COL32(30, 30, 34, 255) : IM_COL32(34, 34, 39, 255);
			if (static_cast<int>(trackIndex) == selectedTrack)
			{
				background = IM_COL32(44, 48, 62, 255);
			}
			if (track->IsMuted()) { background = IM_COL32(23, 23, 26, 255); }
			drawList->AddRectFilled(ImVec2(canvasMin.x, rowTop), ImVec2(canvasMin.x + canvasSize.x, rowBottom), background);

			// トラック名欄
			char headerLabel[128];
			if (row == 0)
			{
				const bool collapsed = std::find(collapsedTracks_.begin(), collapsedTracks_.end(), trackIndex) != collapsedTracks_.end();
				std::snprintf(headerLabel, sizeof(headerLabel), "%s %s%s", collapsed ? ">" : "v", hasBinding ? "" : "! ", track->GetName().c_str());
			}
			else if (channel)
			{
				std::snprintf(headerLabel, sizeof(headerLabel), "    %s", channel->GetName());
			}
			else
			{
				std::snprintf(headerLabel, sizeof(headerLabel), "%s", track->GetName().c_str());
			}
			drawList->AddText(
				ImVec2(canvasMin.x + 6.0f, rowCenter - ImGui::GetTextLineHeight() * 0.5f),
				track->IsMuted() ? IM_COL32(110, 110, 115, 255) : IM_COL32(215, 215, 225, 255),
				headerLabel);
			if (!hasBinding && row == 0)
			{
				drawList->AddText(
					ImVec2(canvasMin.x + 6.0f, rowCenter + ImGui::GetTextLineHeight() * 0.15f),
					IM_COL32(255, 150, 80, 255), "対象が未割り当て");
			}

			// ミュートの切り替えボタン。先頭の行にだけ置く
			if (row == 0)
			{
				ImGui::SetCursorScreenPos(ImVec2(canvasMin.x + view_.headerWidth - 30.0f, rowTop + 2.0f));
				ImGui::PushID(static_cast<int>(trackIndex));
				if (ImGui::SmallButton(track->IsMuted() ? "M" : "-"))
				{
					ExecuteTrackEdit(sequence_, trackIndex, "Toggle Mute", [track]() { track->SetMuted(!track->IsMuted()); });
					player_.EvaluateCurrentTime();
				}
				ImGui::PopID();
			}

			if (row == 0)
			{
				const ImU32 typeColor = [&]()
				{
					switch (track->GetType())
					{
					case TrackType::Camera: return IM_COL32(80, 130, 210, 255);
					case TrackType::Transform: return IM_COL32(80, 175, 130, 255);
					case TrackType::Light: return IM_COL32(210, 175, 70, 255);
					case TrackType::PostProcess: return IM_COL32(170, 90, 190, 255);
					case TrackType::Event: return IM_COL32(215, 100, 85, 255);
					case TrackType::Screen: return IM_COL32(80, 175, 185, 255);
					case TrackType::Text:
					case TrackType::Text3D: return IM_COL32(185, 130, 200, 255);
					case TrackType::Component: return IM_COL32(200, 125, 70, 255);
					default: return IM_COL32(110, 110, 120, 255);
					}
				}();
				drawList->AddRectFilled(ImVec2(canvasMin.x, rowTop), ImVec2(canvasMin.x + 4.0f, rowBottom), typeColor);
				for (size_t channelIndex = 0; channelIndex < track->GetChannelCount(); ++channelIndex)
				{
					ICurveChannel* aggregate = track->GetChannel(channelIndex);
					for (size_t keyIndex = 0; aggregate && keyIndex < aggregate->GetKeyCount(); ++keyIndex)
					{
						const float x = TimeToPixel(aggregate->GetKeyTime(keyIndex), left);
						drawList->AddCircleFilled(ImVec2(x, rowCenter), 3.0f, typeColor);
					}
				}
				rowY = rowBottom;
				continue;
			}

			if (!channel)
			{
				rowY = rowBottom;
				continue;
			}

			// キーの描画
			for (size_t keyIndex = 0; keyIndex < channel->GetKeyCount(); ++keyIndex)
			{
				const float x = TimeToPixel(channel->GetKeyTime(keyIndex), left);
				if (x < left - kKeyMarkerRadius || x > canvasMin.x + canvasSize.x + kKeyMarkerRadius)
				{
					continue;
				}

				SelectionItem item;
				item.kind = SelectionKind::SequenceKey;
				item.trackIndex = static_cast<int>(trackIndex);
				item.channelIndex = static_cast<int>(row - 1);
				item.keyIndex = static_cast<int>(keyIndex);

				const bool isSelected = selection->IsSelected(item);

				const ImU32 keyColor = isSelected ? IM_COL32(255, 200, 80, 255) : IM_COL32(150, 190, 255, 255);
				const InterpolationMode interp = channel->HasInterpolation() ? channel->GetKeyInterp(keyIndex) : InterpolationMode::Constant;
				if (interp == InterpolationMode::Linear)
				{
					drawList->AddCircleFilled(ImVec2(x, rowCenter), kKeyMarkerRadius, keyColor);
				}
				else if (interp == InterpolationMode::Constant)
				{
					drawList->AddRectFilled(ImVec2(x - kKeyMarkerRadius, rowCenter - kKeyMarkerRadius), ImVec2(x + kKeyMarkerRadius, rowCenter + kKeyMarkerRadius), keyColor);
				}
				else
				{
					const ImVec2 points[4] = { ImVec2(x, rowCenter - kKeyMarkerRadius), ImVec2(x + kKeyMarkerRadius, rowCenter), ImVec2(x, rowCenter + kKeyMarkerRadius), ImVec2(x - kKeyMarkerRadius, rowCenter) };
					drawList->AddConvexPolyFilled(points, 4, keyColor);
					drawList->AddPolyline(points, 4, IM_COL32(20, 20, 25, 255), ImDrawFlags_Closed, 1.0f);
				}
				if (std::abs(ImGui::GetIO().MousePos.x - x) <= kKeyGrabRadius && std::abs(ImGui::GetIO().MousePos.y - rowCenter) <= kKeyGrabRadius)
				{
					ImGui::BeginTooltip();
					ImGui::Text("時刻: %.3f s", channel->GetKeyTime(keyIndex));
					const nlohmann::json value = channel->CopyKey(keyIndex);
					if (!value.is_null()) { ImGui::TextWrapped("値: %s", value.dump().c_str()); }
					ImGui::Text("補間: %s", interp == InterpolationMode::Constant ? "一定" : (interp == InterpolationMode::Linear ? "直線" : "ベジェ"));
					ImGui::EndTooltip();
				}
			}
			rowY = rowBottom;
		}
	}

	// --- クリックによる選択 ---
	const ImVec2 mousePos = ImGui::GetIO().MousePos;
	const float rowsTop = canvasMin.y + view_.rulerHeight;
	const bool mouseInRows =
		mousePos.x >= canvasMin.x && mousePos.x <= canvasMin.x + canvasSize.x &&
		mousePos.y >= rowsTop && mousePos.y <= canvasMin.y + canvasSize.y;
	const bool mouseInHeader = mouseInRows && mousePos.x < left;
	auto findHitRow = [&]()
	{
		std::pair<int, int> hit{ -1, -1 };
		float searchRowY = rowsTop - view_.verticalScroll;
		for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount() && hit.first < 0; ++trackIndex)
		{
			ITrack* track = sequence_.GetTrack(trackIndex);
			if (!track) { continue; }
			for (size_t row = 0; row < GetVisibleRowCount(trackIndex); ++row, searchRowY += view_.trackHeight)
			{
				if (mousePos.y >= searchRowY && mousePos.y < searchRowY + view_.trackHeight)
				{
					hit = { static_cast<int>(trackIndex), static_cast<int>(row) };
					break;
				}
			}
		}
		return hit;
	};

	if (mouseInRows && !mouseInHeader && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		const auto [trackIndex, displayRow] = findHitRow();
		const int channelIndex = displayRow - 1;
		ITrack* track = trackIndex >= 0 ? sequence_.GetTrack(static_cast<size_t>(trackIndex)) : nullptr;
		ICurveChannel* channel = track && channelIndex >= 0 ? track->GetChannel(static_cast<size_t>(channelIndex)) : nullptr;
		if (channel)
		{
			const float time = (std::max)(SnapTime(PixelToTime(mousePos.x, left)), 0.0f);
			bool added = false;
			ExecuteTrackEdit(sequence_, static_cast<size_t>(trackIndex), "Add Key", [&]()
			{
				if (channel->IsEmpty() && HasRequiredBinding(*track, player_.GetBindingContext()))
				{
					track->RecordKey(time, player_.GetBindingContext());
					added = !channel->IsEmpty();
				}
				if (!added) { added = channel->AddKeyAt(time); }
			});
			statusMessage_ = added ? "キーを追加しました" : "この行にはキーを追加できません";
			player_.EvaluateCurrentTime();
		}
	}

	if (mouseInRows && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		const auto [trackIndex, displayRow] = findHitRow();
		const int channelIndex = displayRow - 1;
		contextTrackIndex_ = trackIndex;
		contextChannelIndex_ = channelIndex;
		contextTime_ = (std::max)(SnapTime(PixelToTime(mousePos.x, left)), 0.0f);
		if (trackIndex >= 0 && channelIndex >= 0 && !mouseInHeader)
		{
			ITrack* track = sequence_.GetTrack(static_cast<size_t>(trackIndex));
			ICurveChannel* channel = track ? track->GetChannel(static_cast<size_t>(channelIndex)) : nullptr;
			float bestDistance = kKeyGrabRadius;
			SelectionItem picked;
			for (size_t keyIndex = 0; channel && keyIndex < channel->GetKeyCount(); ++keyIndex)
			{
				const float distance = std::abs(TimeToPixel(channel->GetKeyTime(keyIndex), left) - mousePos.x);
				if (distance < bestDistance)
				{
					bestDistance = distance;
					picked.kind = SelectionKind::SequenceKey;
					picked.trackIndex = trackIndex;
					picked.channelIndex = channelIndex;
					picked.keyIndex = static_cast<int>(keyIndex);
				}
			}
			if (picked.kind == SelectionKind::SequenceKey) { selection->Select(picked); }
		}
		ImGui::OpenPopup("TimelineContextMenu");
	}
	DrawTimelineContextMenu();

	// ミュートボタンなど他のウィジェットを押した場合は選択を変えない
	if (mouseInRows && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
		!draggingPlayhead_ && !ImGui::IsAnyItemHovered())
	{
		const auto [hitTrack, displayRow] = findHitRow();
		const int hitChannel = displayRow - 1;
		if (hitTrack >= 0 && displayRow == 0 && mouseInHeader && mousePos.x < canvasMin.x + 26.0f)
		{
			const size_t index = static_cast<size_t>(hitTrack);
			const auto collapsed = std::find(collapsedTracks_.begin(), collapsedTracks_.end(), index);
			if (collapsed == collapsedTracks_.end()) { collapsedTracks_.push_back(index); }
			else { collapsedTracks_.erase(collapsed); }
		}

		SelectionItem picked;
		if (hitTrack >= 0 && !mouseInHeader && hitChannel >= 0)
		{
			// その行で最も近いキーを拾う
			ITrack* track = sequence_.GetTrack(static_cast<size_t>(hitTrack));
			ICurveChannel* channel = track ? track->GetChannel(static_cast<size_t>(hitChannel)) : nullptr;
			float bestDistance = kKeyGrabRadius;
			for (size_t keyIndex = 0; channel && keyIndex < channel->GetKeyCount(); ++keyIndex)
			{
				const float distance = std::abs(TimeToPixel(channel->GetKeyTime(keyIndex), left) - mousePos.x);
				if (distance < bestDistance)
				{
					bestDistance = distance;
					picked.kind = SelectionKind::SequenceKey;
					picked.trackIndex = hitTrack;
					picked.channelIndex = hitChannel;
					picked.keyIndex = static_cast<int>(keyIndex);
				}
			}
		}
		else if (hitTrack >= 0 && !mouseInHeader && displayRow == 0)
		{
			ITrack* track = sequence_.GetTrack(static_cast<size_t>(hitTrack));
			float bestDistance = kKeyGrabRadius;
			float pickedTime = 0.0f;
			for (size_t channelIndex = 0; track && channelIndex < track->GetChannelCount(); ++channelIndex)
			{
				ICurveChannel* aggregate = track->GetChannel(channelIndex);
				for (size_t keyIndex = 0; aggregate && keyIndex < aggregate->GetKeyCount(); ++keyIndex)
				{
					const float distance = std::abs(TimeToPixel(aggregate->GetKeyTime(keyIndex), left) - mousePos.x);
					if (distance < bestDistance) { bestDistance = distance; pickedTime = aggregate->GetKeyTime(keyIndex); }
				}
			}
			if (bestDistance < kKeyGrabRadius)
			{
				selection->ClearSelection();
				for (size_t channelIndex = 0; channelIndex < track->GetChannelCount(); ++channelIndex)
				{
					ICurveChannel* aggregate = track->GetChannel(channelIndex);
					const int keyIndex = aggregate ? aggregate->FindKeyAt(pickedTime, kPastedKeyTolerance) : -1;
					if (keyIndex < 0) { continue; }
					SelectionItem item;
					item.kind = SelectionKind::SequenceKey;
					item.trackIndex = hitTrack;
					item.channelIndex = static_cast<int>(channelIndex);
					item.keyIndex = keyIndex;
					selection->AddToSelection(item);
					picked = item;
				}
			}
		}

		const bool additive = ImGui::GetIO().KeyCtrl;
		if (picked.kind == SelectionKind::SequenceKey)
		{
			if (additive)
			{
				// Ctrl+クリックは選択の足し引きだけ。掴まない
				selection->ToggleSelection(picked);
			}
			else
			{
				// 選択中のキーを掴んだら、複数選択のまま全部動かす。掴んだキーを主選択にしておく
				if (selection->IsSelected(picked))
				{
					selection->RemoveFromSelection(picked);
					selection->AddToSelection(picked);
				}
				else
				{
					selection->Select(picked);
				}
				BeginKeyDrag(picked);
			}
		}
		else
		{
			if (!additive)
			{
				if (hitTrack >= 0)
				{
					// キーの無い所をクリックしたらトラックを選ぶ。インスペクタで役を割り当てるため
					SelectionItem trackItem;
					trackItem.kind = SelectionKind::SequenceTrack;
					trackItem.trackIndex = hitTrack;
					selection->Select(trackItem);
				}
				else
				{
					selection->ClearSelection();
				}
			}
			// キーの無い所から引っ張ったら範囲選択にする
			if (!mouseInHeader)
			{
				boxSelecting_ = true;
				boxStart_ = mousePos;
			}
		}
	}

	// --- キーのドラッグ ---
	if (draggingKey_)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				UpdateKeyDrag(PixelToTime(mousePos.x, left));
			}
		}
		else
		{
			EndKeyDrag();
		}
	}

	// --- 範囲選択 ---
	if (boxSelecting_)
	{
		const ImVec2 rectMin((std::min)(boxStart_.x, mousePos.x), (std::min)(boxStart_.y, mousePos.y));
		const ImVec2 rectMax((std::max)(boxStart_.x, mousePos.x), (std::max)(boxStart_.y, mousePos.y));
		const bool dragged = rectMax.x - rectMin.x >= kBoxSelectMinDrag || rectMax.y - rectMin.y >= kBoxSelectMinDrag;
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			if (dragged)
			{
				drawList->AddRectFilled(rectMin, rectMax, IM_COL32(120, 170, 255, 40));
				drawList->AddRect(rectMin, rectMax, IM_COL32(120, 170, 255, 200));
			}
		}
		else
		{
			// 動かさずに離したときは、上のクリックの選択をそのまま使う
			if (dragged)
			{
				SelectKeysInRect(canvasMin, rectMin, rectMax, ImGui::GetIO().KeyCtrl);
			}
			boxSelecting_ = false;
		}
	}
	drawList->PopClipRect();
}

void SequencerEditor::BeginKeyDrag(const SelectionItem& grabbed)
{
	draggedKeys_.clear();
	dragCommands_.clear();
	grabbedKey_ = 0;
	for (const SelectionItem& item : CollectSelectedKeys(sequence_))
	{
		// 編集前の状態はトラックごとに1回だけ取る
		const bool trackRecorded = std::any_of(draggedKeys_.begin(), draggedKeys_.end(),
			[&item](const DraggedKey& other) { return other.trackIndex == item.trackIndex; });
		if (!trackRecorded)
		{
			dragCommands_.push_back(std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(item.trackIndex), "Move Keys"));
		}

		DraggedKey key;
		key.trackIndex = item.trackIndex;
		key.channelIndex = item.channelIndex;
		key.keyIndex = static_cast<size_t>(item.keyIndex);
		key.originalTime = GetSelectedChannel(sequence_, item)->GetKeyTime(key.keyIndex);
		if (item == grabbed)
		{
			grabbedKey_ = draggedKeys_.size();
		}
		draggedKeys_.push_back(key);
	}
	draggingKey_ = !draggedKeys_.empty();
}

void SequencerEditor::UpdateKeyDrag(float mouseTime)
{
	if (draggedKeys_.empty())
	{
		return;
	}

	// 動かす量は掴んだキーで決める。どのキーも 0 秒より前には出さない
	float earliest = draggedKeys_.front().originalTime;
	for (const DraggedKey& key : draggedKeys_)
	{
		earliest = (std::min)(earliest, key.originalTime);
	}
	const float delta = (std::max)(SnapTime(mouseTime) - draggedKeys_[grabbedKey_].originalTime, -earliest);

	for (size_t i = 0; i < draggedKeys_.size(); ++i)
	{
		DraggedKey& key = draggedKeys_[i];
		ITrack* track = sequence_.GetTrack(static_cast<size_t>(key.trackIndex));
		ICurveChannel* channel = track ? track->GetChannel(static_cast<size_t>(key.channelIndex)) : nullptr;
		if (!channel || key.keyIndex >= channel->GetKeyCount())
		{
			continue;
		}

		const size_t oldIndex = key.keyIndex;
		const size_t newIndex = channel->MoveKey(oldIndex, key.originalTime + delta);
		key.keyIndex = newIndex;
		// MoveKey は抜いて入れ直すので、同じチャンネルの他のキーはその分だけ番号がずれる
		for (size_t j = 0; j < draggedKeys_.size(); ++j)
		{
			DraggedKey& other = draggedKeys_[j];
			if (j == i || other.trackIndex != key.trackIndex || other.channelIndex != key.channelIndex)
			{
				continue;
			}
			if (other.keyIndex > oldIndex) { --other.keyIndex; }
			if (other.keyIndex >= newIndex) { ++other.keyIndex; }
		}
	}

	// 並べ替えで番号が変わるので、選択を作り直す。掴んだキーを最後に足して主選択にする
	const auto toItem = [](const DraggedKey& key)
	{
		SelectionItem item;
		item.kind = SelectionKind::SequenceKey;
		item.trackIndex = key.trackIndex;
		item.channelIndex = key.channelIndex;
		item.keyIndex = static_cast<int>(key.keyIndex);
		return item;
	};
	SelectionContext* selection = SelectionContext::GetInstance();
	selection->ClearSelection();
	for (size_t i = 0; i < draggedKeys_.size(); ++i)
	{
		if (i != grabbedKey_)
		{
			selection->AddToSelection(toItem(draggedKeys_[i]));
		}
	}
	selection->AddToSelection(toItem(draggedKeys_[grabbedKey_]));
	player_.EvaluateCurrentTime();
}

void SequencerEditor::EndKeyDrag()
{
	// 何本のトラックにまたがっても、1回の Undo で掴む前に戻せるようにまとめる
	CommandHistory* history = CommandHistory::GetInstance();
	history->BeginTransaction("Move Keys");
	for (auto& command : dragCommands_)
	{
		command->CaptureAfter();
		if (command->HasChanged())
		{
			history->Execute(std::move(command));
		}
	}
	history->EndTransaction();
	dragCommands_.clear();
	draggedKeys_.clear();
	draggingKey_ = false;
}

void SequencerEditor::SelectKeysInRect(const ImVec2& canvasMin, const ImVec2& rectMin, const ImVec2& rectMax, bool additive)
{
	SelectionContext* selection = SelectionContext::GetInstance();
	if (!additive)
	{
		selection->ClearSelection();
	}

	const float left = canvasMin.x + view_.headerWidth;
	float rowY = canvasMin.y + view_.rulerHeight - view_.verticalScroll;
	for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount(); ++trackIndex)
	{
		ITrack* track = sequence_.GetTrack(trackIndex);
		if (!track)
		{
			continue;
		}
		// 見出し行を飛ばし、展開中のチャンネル行だけを範囲選択する
		rowY += view_.trackHeight;
		for (size_t row = 1; row < GetVisibleRowCount(trackIndex); ++row, rowY += view_.trackHeight)
		{
			// キーは行の真ん中に描いているので、真ん中が四角に入った行だけを見る
			const float rowCenter = rowY + view_.trackHeight * 0.5f;
			ICurveChannel* channel = track->GetChannel(row - 1);
			if (!channel || rowCenter < rectMin.y || rowCenter > rectMax.y)
			{
				continue;
			}
			for (size_t keyIndex = 0; keyIndex < channel->GetKeyCount(); ++keyIndex)
			{
				const float x = TimeToPixel(channel->GetKeyTime(keyIndex), left);
				if (x < rectMin.x || x > rectMax.x)
				{
					continue;
				}
				SelectionItem item;
				item.kind = SelectionKind::SequenceKey;
				item.trackIndex = static_cast<int>(trackIndex);
				item.channelIndex = static_cast<int>(row - 1);
				item.keyIndex = static_cast<int>(keyIndex);
				selection->AddToSelection(item);
			}
		}
	}
}

void SequencerEditor::DrawPlayhead(const ImVec2& canvasMin, const ImVec2& canvasSize)
{
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float left = canvasMin.x + view_.headerWidth;
	const float x = TimeToPixel(player_.GetTime(), left);

	if (x >= left && x <= canvasMin.x + canvasSize.x)
	{
		drawList->AddLine(
			ImVec2(x, canvasMin.y),
			ImVec2(x, canvasMin.y + canvasSize.y),
			IM_COL32(255, 90, 90, 255), 1.5f);

		// 掴みやすいように頭に三角を置く
		const ImVec2 head[3] = {
			ImVec2(x - 6.0f, canvasMin.y),
			ImVec2(x + 6.0f, canvasMin.y),
			ImVec2(x, canvasMin.y + 10.0f),
		};
		drawList->AddConvexPolyFilled(head, 3, IM_COL32(255, 90, 90, 255));
		char timeLabel[32];
		FormatTime(timeLabel, sizeof(timeLabel), player_.GetTime());
		drawList->AddRectFilled(ImVec2(x + 7.0f, canvasMin.y), ImVec2(x + 76.0f, canvasMin.y + 18.0f), IM_COL32(95, 35, 35, 230));
		drawList->AddText(ImVec2(x + 10.0f, canvasMin.y + 2.0f), IM_COL32(255, 235, 235, 255), timeLabel);
	}

	// ルーラー上のドラッグでスクラブする
	const ImVec2 mousePos = ImGui::GetIO().MousePos;
	const bool inRuler =
		mousePos.x >= left && mousePos.x <= canvasMin.x + canvasSize.x &&
		mousePos.y >= canvasMin.y && mousePos.y <= canvasMin.y + view_.rulerHeight;

	if (inRuler && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		draggingPlayhead_ = true;
	}

	if (draggingPlayhead_)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			// スクラブは Seek するだけ。純関数契約が守られていれば
			// これだけで前後どちらへ動かしても正しい絵になる。
			player_.Seek((std::max)(SnapTime(PixelToTime(mousePos.x, left)), 0.0f));
		}
		else
		{
			draggingPlayhead_ = false;
		}
	}
}

void SequencerEditor::DrawTimelineWindow()
{
	DrawToolbar();
	ImGui::Separator();

	// メタ情報
	if (ImGui::CollapsingHeader("Sequence Settings"))
	{
		SequenceMeta& meta = sequence_.GetMeta();

		char nameBuffer[128];
		std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", meta.name.c_str());
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
		{
			meta.name = nameBuffer;
		}

		const char* audioPreview = meta.audioClip.empty() ? "(None)" : meta.audioClip.c_str();
		if (ImGui::BeginCombo("Audio Clip", audioPreview))
		{
			if (ImGui::Selectable("(None)", meta.audioClip.empty()))
			{
				++metaEditId_;
				SequenceMeta after = meta;
				after.audioClip.clear();
				ExecuteMetaEdit(sequence_, after, "Change Audio Clip", metaEditId_);
			}
			for (const std::string& soundName : Audio::GetInstance()->GetLoadedSoundNames())
			{
				if (ImGui::Selectable(soundName.c_str(), soundName == meta.audioClip))
				{
					++metaEditId_;
					SequenceMeta after = meta;
					after.audioClip = soundName;
					ExecuteMetaEdit(sequence_, after, "Change Audio Clip", metaEditId_);
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Audio に読み込み済みの音声名。\n設定するとこの音声の再生位置が時刻の権威になります");
		}

		float bpm = meta.bpm;
		const bool bpmChanged = ImGui::DragFloat("BPM", &bpm, 0.1f, 0.0f, 400.0f, "%.2f");
		// つかんだフレームで必ず番号を進める。値が変わるのが次のフレームでも、前の操作とまとめないため
		if (ImGui::IsItemActivated()) { ++metaEditId_; }
		if (bpmChanged)
		{
			SequenceMeta after = meta;
			after.bpm = bpm;
			ExecuteMetaEdit(sequence_, after, "Change BPM", metaEditId_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Tap Tempo"))
		{
			++metaEditId_;
			const double now = ImGui::GetTime();
			if (tapCount_ > 0 && now - tapTimes_[tapCount_ - 1] > kTapResetSeconds)
			{
				tapCount_ = 0;
			}
			if (tapCount_ == tapTimes_.size())
			{
				std::move(tapTimes_.begin() + 1, tapTimes_.end(), tapTimes_.begin());
				--tapCount_;
			}
			tapTimes_[tapCount_++] = now;
			if (tapCount_ >= 2)
			{
				const double averageInterval = (tapTimes_[tapCount_ - 1] - tapTimes_[0]) / static_cast<double>(tapCount_ - 1);
				SequenceMeta after = meta;
				after.bpm = std::clamp(static_cast<float>(60.0 / averageInterval), 0.0f, 400.0f);
				ExecuteMetaEdit(sequence_, after, "Tap Tempo", metaEditId_);
			}
		}

		float offset = meta.offset;
		const bool offsetChanged = ImGui::DragFloat("Offset", &offset, 0.001f, -10.0f, 10.0f, "%.3f s");
		if (ImGui::IsItemActivated()) { ++metaEditId_; }
		if (offsetChanged)
		{
			SequenceMeta after = meta;
			after.offset = offset;
			ExecuteMetaEdit(sequence_, after, "Change Offset", metaEditId_);
		}
		ImGui::SameLine();
		if (ImGui::Button("Set to Playhead"))
		{
			++metaEditId_;
			SequenceMeta after = meta;
			after.offset = player_.GetTime();
			ExecuteMetaEdit(sequence_, after, "Set Offset to Playhead", metaEditId_);
		}
		ImGui::DragFloat("Duration", &meta.duration, 0.1f, 0.0f, 3600.0f, "%.2f s");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("0 にするとトラックの最終キーから自動で決まります");
		}
	}

	// ズーム
	ImGui::SetNextItemWidth(200.0f);
	ImGui::DragFloat("Zoom", &view_.pixelsPerSecond, 1.0f, 5.0f, 2000.0f, "%.0f px/s");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	ImGui::DragFloat("Scroll", &view_.scrollTime, 0.05f, 0.0f, 3600.0f, "%.2f s");
	ImGui::SameLine();
	if (ImGui::Button("全体を表示 (F)"))
	{
		FrameAllKeys();
	}

	ImGui::Separator();

	// --- キャンバス ---
	const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
	ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	if (canvasSize.x < 50.0f || canvasSize.y < 50.0f)
	{
		return;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasMin, ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y), IM_COL32(26, 26, 30, 255));
	if (sequence_.GetTrackCount() == 0)
	{
		drawList->AddText(
			ImVec2(canvasMin.x + view_.headerWidth + 24.0f, canvasMin.y + view_.rulerHeight + 24.0f),
			IM_COL32(190, 190, 200, 255),
			"トラックを追加 → 対象を割り当て → 行をダブルクリックでキー");
	}

	// トラック名欄との境界
	drawList->AddLine(
		ImVec2(canvasMin.x + view_.headerWidth, canvasMin.y),
		ImVec2(canvasMin.x + view_.headerWidth, canvasMin.y + canvasSize.y),
		IM_COL32(70, 70, 80, 255));

	const ImVec2 mousePos = ImGui::GetIO().MousePos;
	timelineHovered_ = mousePos.x >= canvasMin.x && mousePos.x <= canvasMin.x + canvasSize.x &&
		mousePos.y >= canvasMin.y && mousePos.y <= canvasMin.y + canvasSize.y;

	float totalRowsHeight = 0.0f;
	for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount(); ++trackIndex)
	{
		totalRowsHeight += static_cast<float>(GetVisibleRowCount(trackIndex)) * view_.trackHeight;
	}
	const float visibleRowsHeight = (std::max)(canvasSize.y - view_.rulerHeight - ImGui::GetFrameHeightWithSpacing(), 1.0f);
	const float maxVerticalScroll = (std::max)(totalRowsHeight - visibleRowsHeight, 0.0f);
	if (pendingScrollToTrack_ >= 0)
	{
		float targetTop = 0.0f;
		for (int index = 0; index < pendingScrollToTrack_ && index < static_cast<int>(sequence_.GetTrackCount()); ++index)
		{
			ITrack* track = sequence_.GetTrack(static_cast<size_t>(index));
			if (track) { targetTop += static_cast<float>(GetVisibleRowCount(static_cast<size_t>(index))) * view_.trackHeight; }
		}
		ITrack* target = sequence_.GetTrack(static_cast<size_t>(pendingScrollToTrack_));
		const float targetBottom = targetTop + (target ? static_cast<float>(GetVisibleRowCount(static_cast<size_t>(pendingScrollToTrack_))) * view_.trackHeight : 0.0f);
		if (targetBottom > view_.verticalScroll + visibleRowsHeight) { view_.verticalScroll = targetBottom - visibleRowsHeight; }
		if (targetTop < view_.verticalScroll) { view_.verticalScroll = targetTop; }
		pendingScrollToTrack_ = -1;
	}

	// 一般的な動画編集ソフトと同じホイール操作にする
	if (timelineHovered_ && ImGui::GetIO().MouseWheel != 0.0f)
	{
		const float wheel = ImGui::GetIO().MouseWheel;
		if (ImGui::GetIO().KeyCtrl)
		{
			// Ctrl+ホイールでズーム。マウス位置の時刻を保ったまま拡大縮小する
			const float left = canvasMin.x + view_.headerWidth;
			const float timeUnderMouse = PixelToTime(ImGui::GetIO().MousePos.x, left);
			view_.pixelsPerSecond = std::clamp(view_.pixelsPerSecond * (1.0f + wheel * 0.1f), 5.0f, 2000.0f);
			view_.scrollTime = timeUnderMouse - (ImGui::GetIO().MousePos.x - left) / view_.pixelsPerSecond;
		}
		else if (ImGui::GetIO().KeyShift)
		{
			view_.scrollTime -= wheel * (100.0f / view_.pixelsPerSecond);
		}
		else
		{
			view_.verticalScroll -= wheel * view_.trackHeight * 3.0f;
		}
		view_.scrollTime = (std::max)(view_.scrollTime, 0.0f);
	}

	if (timelineHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) { panningTimeline_ = true; }
	if (panningTimeline_)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			const ImVec2 delta = ImGui::GetIO().MouseDelta;
			view_.scrollTime = (std::max)(view_.scrollTime - delta.x / view_.pixelsPerSecond, 0.0f);
			view_.verticalScroll -= delta.y;
		}
		else { panningTimeline_ = false; }
	}
	view_.verticalScroll = std::clamp(view_.verticalScroll, 0.0f, maxVerticalScroll);

	DrawRuler(canvasMin, canvasSize.x);
	DrawWaveform(canvasMin, canvasSize.x);
	DrawTracks(canvasMin, canvasSize);
	DrawPlayhead(canvasMin, canvasSize);

	ImGui::SetCursorScreenPos(ImVec2(canvasMin.x + view_.headerWidth, canvasMin.y + canvasSize.y - ImGui::GetFrameHeight()));
	ImGui::SetNextItemWidth(canvasSize.x - view_.headerWidth);
	const float visibleSeconds = (std::max)((canvasSize.x - view_.headerWidth) / view_.pixelsPerSecond, 0.01f);
	const float maxScrollTime = (std::max)(sequence_.GetDuration() - visibleSeconds, 0.0f);
	ImGui::SliderFloat("##TimeScrollbar", &view_.scrollTime, 0.0f, maxScrollTime, "", ImGuiSliderFlags_AlwaysClamp);

	// キャンバス領域を確保しておかないと、後続のUIが重なって描かれる
	ImGui::SetCursorScreenPos(canvasMin);
	ImGui::Dummy(canvasSize);
}

///=============================================================================
///						インスペクタ
///=============================================================================

void SequencerEditor::DrawBezierEditor()
{
	const SelectionItem selected = GetPrimarySelection();
	ICurveChannel* channel = GetSelectedChannel(sequence_, selected);
	if (!channel)
	{
		return;
	}

	// イベントのように補間を持たないキーには、補間の編集UIを出さない
	if (!channel->HasInterpolation())
	{
		return;
	}

	const size_t trackIndex = static_cast<size_t>(selected.trackIndex);
	const size_t keyIndex = static_cast<size_t>(selected.keyIndex);
	InterpolationMode& interp = channel->GetKeyInterp(keyIndex);
	BezierHandle& bezier = channel->GetKeyBezier(keyIndex);

	ImGui::SeparatorText("Interpolation (このキーから次のキーまで)");

	int interpIndex = static_cast<int>(interp);
	if (ImGui::Combo("Mode", &interpIndex, "Constant\0Linear\0Bezier\0"))
	{
		ExecuteTrackEdit(sequence_, trackIndex, "Change Interpolation", [&]() { interp = static_cast<InterpolationMode>(interpIndex); });
		player_.EvaluateCurrentTime();
	}

	if (interp != InterpolationMode::Bezier)
	{
		return;
	}

	// プリセット
	struct Preset { const char* label; EasingType type; };
	static const Preset kPresets[] = {
		{ "Ease In-Out", EasingType::EaseInOutCubic },
		{ "Ease Out", EasingType::EaseOutCubic },
		{ "Ease In", EasingType::EaseInCubic },
		{ "Linear", EasingType::Linear },
	};
	for (size_t i = 0; i < sizeof(kPresets) / sizeof(kPresets[0]); ++i)
	{
		if (i > 0) { ImGui::SameLine(); }
		if (ImGui::Button(kPresets[i].label))
		{
			ExecuteTrackEdit(sequence_, trackIndex, "Set Easing Preset", [&]() { bezier = EasingTypeToBezier(kPresets[i].type); });
			player_.EvaluateCurrentTime();
		}
	}

	// 数値でのハンドル編集
	float handle[4] = { bezier.x1, bezier.y1, bezier.x2, bezier.y2 };
	if (ImGui::DragFloat4("cubic-bezier", handle, 0.01f, -2.0f, 3.0f, "%.3f"))
	{
		ExecuteTrackEdit(sequence_, trackIndex, "Edit Bezier Handle", [&]()
		{
			// x は単調でなければ解が一意に定まらないため 0〜1 に制限する
			bezier.x1 = std::clamp(handle[0], 0.0f, 1.0f);
			bezier.y1 = handle[1];
			bezier.x2 = std::clamp(handle[2], 0.0f, 1.0f);
			bezier.y2 = handle[3];
		});
		player_.EvaluateCurrentTime();
	}

	// カーブのグラフ。制御点をドラッグして形を変えられる
	const ImVec2 graphMin = ImGui::GetCursorScreenPos();
	const ImVec2 graphMax(graphMin.x + kBezierGraphSize, graphMin.y + kBezierGraphSize);
	const auto toScreen = [&graphMin](float x, float y)
	{
		const float normalizedY = (kBezierGraphMaxValue - y) / (kBezierGraphMaxValue - kBezierGraphMinValue);
		return ImVec2(graphMin.x + x * kBezierGraphSize, graphMin.y + normalizedY * kBezierGraphSize);
	};
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(graphMin, graphMax, IM_COL32(20, 20, 24, 255));
	drawList->AddRect(graphMin, graphMax, IM_COL32(80, 80, 90, 255));
	// 値の 0 と 1 の目安線。これより外は行き過ぎ
	drawList->AddLine(toScreen(0.0f, 0.0f), toScreen(1.0f, 0.0f), IM_COL32(60, 60, 70, 255));
	drawList->AddLine(toScreen(0.0f, 1.0f), toScreen(1.0f, 1.0f), IM_COL32(60, 60, 70, 255));

	constexpr int kPreviewSegments = 48;
	ImVec2 previousPoint = toScreen(0.0f, 0.0f);
	for (int i = 1; i <= kPreviewSegments; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(kPreviewSegments);
		const ImVec2 point = toScreen(t, ApplyBezierEasing(bezier, t));
		drawList->AddLine(previousPoint, point, IM_COL32(120, 200, 255, 255), 1.5f);
		previousPoint = point;
	}

	// 制御点。P1 は始点 (0,0) から、P2 は終点 (1,1) から伸ばす
	const ImVec2 handle1 = toScreen(bezier.x1, bezier.y1);
	const ImVec2 handle2 = toScreen(bezier.x2, bezier.y2);
	drawList->AddLine(toScreen(0.0f, 0.0f), handle1, IM_COL32(200, 200, 210, 160));
	drawList->AddLine(toScreen(1.0f, 1.0f), handle2, IM_COL32(200, 200, 210, 160));
	drawList->AddCircleFilled(handle1, kBezierHandleRadius, bezierDragHandle_ == 1 ? IM_COL32(255, 200, 80, 255) : IM_COL32(230, 230, 240, 255));
	drawList->AddCircleFilled(handle2, kBezierHandleRadius, bezierDragHandle_ == 2 ? IM_COL32(255, 200, 80, 255) : IM_COL32(230, 230, 240, 255));

	ImGui::InvisibleButton("##bezier_graph", ImVec2(kBezierGraphSize, kBezierGraphSize));
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	if (ImGui::IsItemActivated())
	{
		// 近いほうの制御点を掴む。どちらも遠ければ掴まない
		const auto distanceTo = [&mouse](const ImVec2& point)
		{
			const float dx = point.x - mouse.x;
			const float dy = point.y - mouse.y;
			return std::sqrt(dx * dx + dy * dy);
		};
		const float distance1 = distanceTo(handle1);
		const float distance2 = distanceTo(handle2);
		bezierDragHandle_ = 0;
		if ((std::min)(distance1, distance2) <= kBezierHandleGrabRadius)
		{
			bezierDragHandle_ = distance1 <= distance2 ? 1 : 2;
		}
	}
	if (ImGui::IsItemActive() && bezierDragHandle_ != 0)
	{
		// x は単調でなければ解が一意に定まらないため 0〜1 に制限する
		const float x = std::clamp((mouse.x - graphMin.x) / kBezierGraphSize, 0.0f, 1.0f);
		const float y = std::clamp(
			kBezierGraphMaxValue - (mouse.y - graphMin.y) / kBezierGraphSize * (kBezierGraphMaxValue - kBezierGraphMinValue),
			kBezierGraphMinValue, kBezierGraphMaxValue);
		ExecuteTrackEdit(sequence_, trackIndex, "Edit Bezier Handle", [&]()
		{
			if (bezierDragHandle_ == 1)
			{
				bezier.x1 = x;
				bezier.y1 = y;
			}
			else
			{
				bezier.x2 = x;
				bezier.y2 = y;
			}
		});
		player_.EvaluateCurrentTime();
	}
	if (ImGui::IsItemDeactivated())
	{
		bezierDragHandle_ = 0;
	}
}

void SequencerEditor::DrawTrackInspector(size_t trackIndex)
{
	ITrack* track = sequence_.GetTrack(trackIndex);
	if (!track)
	{
		return;
	}

	ImGui::SeparatorText("Track");
	ImGui::Text("Type: %s", track->GetTypeName());

	// 名前
	char nameBuffer[128];
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", track->GetName().c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		const std::string newName = nameBuffer;
		ExecuteTrackEdit(sequence_, trackIndex, "Rename Track", [&]() { track->SetName(newName); });
	}

	// トラック固有の設定。変更はコマンドとして積む
	{
		bool changed = false;
		ExecuteTrackEdit(sequence_, trackIndex, "Edit Track Settings", [&]()
		{
			if (auto* componentTrack = dynamic_cast<ComponentTrack*>(track))
			{
				const auto binding = previewObjectBindings_.find(track->GetBindingRole());
				GameObject* object = binding != previewObjectBindings_.end() && GameObjectManager::HasInstance()
					? GameObjectManager::GetInstance()->FindByGuid(binding->second)
					: nullptr;
				changed = componentTrack->DrawInspectorForObject(object);
			}
			else
			{
				changed = track->DrawInspector();
			}
		});
		if (changed)
		{
			player_.EvaluateCurrentTime();
		}
	}

	BindingType bindingType;
	if (!GetBindingTypeForTrack(*track, bindingType))
	{
		ImGui::TextDisabled("このトラックは役を使いません");
		return;
	}

	// ライトトラックの平行光源は名前を使わない
	if (auto* lightTrack = dynamic_cast<LightTrack*>(track))
	{
		if (lightTrack->GetKind() == LightTrackKind::Directional)
		{
			ImGui::TextDisabled("平行光源はシーンに1つなので役を使いません");
			return;
		}
	}

	ImGui::SeparatorText("Binding");

	// 役の名前
	char roleBuffer[64];
	std::snprintf(roleBuffer, sizeof(roleBuffer), "%s", track->GetBindingRole().c_str());
	if (ImGui::InputText("Role", roleBuffer, sizeof(roleBuffer)))
	{
		const std::string newRole = roleBuffer;
		ExecuteTrackEdit(sequence_, trackIndex, "Change Role", [&]() { track->SetBindingRole(newRole); });
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("シーケンスは実体ではなく役を指します。\n再生時に役へ実体を割り当てることで、同じ演出を別の対象に使い回せます");
	}

	const std::string& role = track->GetBindingRole();
	if (role.empty())
	{
		return;
	}

	// 役の定義をシーケンスへ登録しておく（再生側が何を割り当てればよいか分かるように）
	BindingDefinition definition;
	definition.role = role;
	definition.type = bindingType;
	sequence_.AddBinding(definition);

	ImGui::TextDisabled("プレビュー用の割り当て（エディタのみ・保存されません）");

	switch (bindingType)
	{
	case BindingType::Camera:
		ImGui::TextDisabled("カメラの役 \"%s\" はシーケンスカメラに割り当て済みです", kCameraRole);
		break;

	case BindingType::GameObject:
		DrawPreviewObjectCombo(role, "Preview Object");
		break;

	case BindingType::Light:
	{
		auto* lightTrack = dynamic_cast<LightTrack*>(track);
		const auto it = previewLightBindings_.find(role);
		const std::string current = (it != previewLightBindings_.end()) ? it->second : std::string();

		if (ImGui::BeginCombo("Preview Light", current.empty() ? "(未割り当て)" : current.c_str()))
		{
			if (ImGui::Selectable("(未割り当て)", current.empty()))
			{
				previewLightBindings_.erase(role);
				player_.GetBindingContext().BindLight(role, "");
			}
			if (lightManager_ && lightTrack)
			{
				// 種類に合ったライトだけを並べる
				auto listLights = [&](const auto& lights)
				{
					for (const auto& [name, light] : lights)
					{
						(void)light;
						if (ImGui::Selectable(name.c_str(), name == current))
						{
							previewLightBindings_[role] = name;
							ApplyPreviewBindings();
							player_.EvaluateCurrentTime();
						}
					}
				};
				if (lightTrack->GetKind() == LightTrackKind::Spot)
				{
					listLights(lightManager_->GetSpotLights());
				}
				else
				{
					listLights(lightManager_->GetPointLights());
				}
			}
			ImGui::EndCombo();
		}
		break;
	}
	}

	// カメラの注目点の役にも、プレビュー用の GameObject を割り当てられるようにする
	if (const auto* cameraTrack = dynamic_cast<const CameraTrack*>(track))
	{
		const std::string* aimRoles[] = { &cameraTrack->GetAimRoleA(), &cameraTrack->GetAimRoleB() };
		for (const std::string* aimRole : aimRoles)
		{
			if (aimRole->empty())
			{
				continue;
			}
			BindingDefinition aimDefinition;
			aimDefinition.role = *aimRole;
			aimDefinition.type = BindingType::GameObject;
			sequence_.AddBinding(aimDefinition);

			const std::string label = "Aim: " + *aimRole;
			ImGui::PushID(aimRole);
			DrawPreviewObjectCombo(*aimRole, label.c_str());
			ImGui::PopID();
		}
	}
}

void SequencerEditor::DrawPreviewObjectCombo(const std::string& role, const char* label)
{
	const auto it = previewObjectBindings_.find(role);
	GameObject* current = (it != previewObjectBindings_.end() && GameObjectManager::HasInstance())
		? GameObjectManager::GetInstance()->FindByGuid(it->second)
		: nullptr;

	if (ImGui::BeginCombo(label, current ? current->GetName().c_str() : "(未割り当て)"))
	{
		if (ImGui::Selectable("(未割り当て)", current == nullptr))
		{
			previewObjectBindings_.erase(role);
			player_.GetBindingContext().BindGameObject(role, nullptr);
		}
		if (GameObjectManager::HasInstance())
		{
			for (GameObject* object : GameObjectManager::GetInstance()->GetGameObjects())
			{
				if (!object) { continue; }
				// 同名オブジェクトを区別できるよう、GUID を ID に使う
				ImGui::PushID(object->GetGuid().ToString().c_str());
				if (ImGui::Selectable(object->GetName().c_str(), object == current))
				{
					previewObjectBindings_[role] = object->GetGuid();
					ApplyPreviewBindings();
					player_.EvaluateCurrentTime();
				}
				ImGui::PopID();
			}
		}
		ImGui::EndCombo();
	}
}

void SequencerEditor::DrawInspectorWindow()
{
	const int trackIndex = GetSelectedTrackIndex();
	if (trackIndex < 0)
	{
		ImGui::TextDisabled("タイムラインでトラックまたはキーを選択してください");
		return;
	}

	const SelectionItem selected = GetPrimarySelection();
	if (ICurveChannel* channel = GetSelectedChannel(sequence_, selected))
	{
		const size_t keyIndex = static_cast<size_t>(selected.keyIndex);

		ImGui::SeparatorText("Key");
		ImGui::Text("Channel: %s  /  Key: %zu", channel->GetName(), keyIndex);

		float keyTime = channel->GetKeyTime(keyIndex);
		if (ImGui::DragFloat("Time", &keyTime, 0.01f, 0.0f, 3600.0f, "%.3f s"))
		{
			size_t newIndex = keyIndex;
			if (ExecuteTrackEdit(sequence_, static_cast<size_t>(trackIndex), "Set Key Time",
				[&]() { newIndex = channel->MoveKey(keyIndex, (std::max)(keyTime, 0.0f)); }))
			{
				SelectionItem updated = selected;
				updated.keyIndex = static_cast<int>(newIndex);
				SelectionContext::GetInstance()->Select(updated);
				player_.EvaluateCurrentTime();
			}
		}
		else
		{
			// 値の型ごとの編集UIはチャンネル自身が描く
			bool changed = false;
			ExecuteTrackEdit(sequence_, static_cast<size_t>(trackIndex), "Edit Key Value",
				[&]() { changed = channel->DrawKeyValueEditor(keyIndex); });
			if (changed)
			{
				player_.EvaluateCurrentTime();
			}

			DrawBezierEditor();
		}
	}

	DrawTrackInspector(static_cast<size_t>(trackIndex));
}

///=============================================================================
///						シーンオーバーレイ（ギズモ）
///=============================================================================

void SequencerEditor::DrawSceneOverlay()
{
	if (!SceneViewContext::HasInstance())
	{
		return;
	}

	const SceneViewRect& rect = SceneViewContext::GetInstance()->GetViewportRect();
	Camera* viewCamera = SceneViewContext::GetInstance()->GetCamera();
	Camera* targetCamera = GetSequenceCamera();

	if (!rect.IsValid() || !viewCamera)
	{
		return;
	}

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(rect.x, rect.y, rect.width, rect.height);

	// ImGuizmo は行ベクトル規約の float[16] を扱う。
	// このエンジンの Matrix4x4 と並びが一致するため、そのまま渡せる。
	const Matrix4x4 view = viewCamera->GetViewMatrix();
	const Matrix4x4 projection = viewCamera->GetProjectionMatrix();

	// GameObject を選んでいれば、シーケンスカメラよりそちらを優先して動かす
	if (GameObject* object = SelectionContext::GetInstance()->GetPrimaryGameObject())
	{
		DrawObjectGizmo(object, view, projection);
		return;
	}
	gizmoWasUsing_ = false;

	if (!targetCamera)
	{
		return;
	}

	// シーケンスカメラ視点で見ているときは、自分自身をギズモで動かすことになり
	// 操作が成立しないので描かない
	if (previewThroughSequenceCamera_ || viewCamera == targetCamera)
	{
		return;
	}

	// CameraManager はアクティブなカメラしか行列を作り直さない。シーケンスのカメラはアクティブでないので、
	// ここで作り直さないとギズモで動かしても古い行列のまま描かれて、元の位置に戻って見える
	targetCamera->Update();
	Matrix4x4 world = targetCamera->GetWorldMatrix();

	// カメラは拡大縮小しないので、回転以外は移動として扱う
	const ImGuizmo::OPERATION operation = (gizmoOperation_ == 1) ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE;
	const ImGuizmo::MODE mode = gizmoWorldSpace_ ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

	if (ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], operation, mode, &world.m[0][0]))
	{
		// 行列から位置と回転を取り出してカメラへ戻す。
		// スケールは扱わないため、回転部を正規化してから使う。
		const Vector3 position = { world.m[3][0], world.m[3][1], world.m[3][2] };

		Vector3 axisX = { world.m[0][0], world.m[0][1], world.m[0][2] };
		Vector3 axisY = { world.m[1][0], world.m[1][1], world.m[1][2] };
		Vector3 axisZ = { world.m[2][0], world.m[2][1], world.m[2][2] };
		axisX = axisX.Normalize();
		axisY = axisY.Normalize();
		axisZ = axisZ.Normalize();

		Matrix4x4 rotationMatrix = MakeIdentity4x4();
		rotationMatrix.m[0][0] = axisX.x; rotationMatrix.m[0][1] = axisX.y; rotationMatrix.m[0][2] = axisX.z;
		rotationMatrix.m[1][0] = axisY.x; rotationMatrix.m[1][1] = axisY.y; rotationMatrix.m[1][2] = axisY.z;
		rotationMatrix.m[2][0] = axisZ.x; rotationMatrix.m[2][1] = axisZ.y; rotationMatrix.m[2][2] = axisZ.z;

		targetCamera->SetTranslate(position);
		targetCamera->SetRotateQuaternion(Quaternion::FromMatrix(rotationMatrix));
	}
}

void SequencerEditor::DrawObjectGizmo(GameObject* object, const Matrix4x4& view, const Matrix4x4& projection)
{
	// これより小さい拡大率は潰れたとみなす。軸の正規化で NaN を出さないため
	constexpr float kMinGizmoScale = 1.0e-4f;

	Transform before;
	before.scale = object->GetScale();
	before.rotate = object->GetRotation();
	before.translate = object->GetPosition();
	Matrix4x4 world = MakeAffineMatrix(before.scale, before.rotate, before.translate);

	ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
	if (gizmoOperation_ == 1)
	{
		operation = ImGuizmo::ROTATE;
	}
	else if (gizmoOperation_ == 2)
	{
		operation = ImGuizmo::SCALE;
	}
	// 拡大縮小はオブジェクト自身の軸に沿ってしかできないので、ワールド指定でもローカルで出す
	const ImGuizmo::MODE mode = (gizmoWorldSpace_ && operation != ImGuizmo::SCALE) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

	const bool changed = ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], operation, mode, &world.m[0][0]);

	// 掴んだ瞬間に番号を進めて、ドラッグ1回ごとに別の Undo にする
	const bool isUsing = ImGuizmo::IsUsing();
	if (isUsing && !gizmoWasUsing_)
	{
		++gizmoDragId_;
	}
	gizmoWasUsing_ = isUsing;

	if (!changed)
	{
		return;
	}

	// 行列から位置・拡大率・回転を取り出す。
	// 回転はエンジンのオイラー角の規約にそろえるため、クォータニオンを経由する
	const auto length = [](const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); };
	const Vector3 axisX = { world.m[0][0], world.m[0][1], world.m[0][2] };
	const Vector3 axisY = { world.m[1][0], world.m[1][1], world.m[1][2] };
	const Vector3 axisZ = { world.m[2][0], world.m[2][1], world.m[2][2] };

	Transform after;
	after.translate = { world.m[3][0], world.m[3][1], world.m[3][2] };
	after.scale = { length(axisX), length(axisY), length(axisZ) };
	if (after.scale.x < kMinGizmoScale || after.scale.y < kMinGizmoScale || after.scale.z < kMinGizmoScale)
	{
		return;
	}

	Matrix4x4 rotationMatrix = MakeIdentity4x4();
	rotationMatrix.m[0][0] = axisX.x / after.scale.x; rotationMatrix.m[0][1] = axisX.y / after.scale.x; rotationMatrix.m[0][2] = axisX.z / after.scale.x;
	rotationMatrix.m[1][0] = axisY.x / after.scale.y; rotationMatrix.m[1][1] = axisY.y / after.scale.y; rotationMatrix.m[1][2] = axisY.z / after.scale.y;
	rotationMatrix.m[2][0] = axisZ.x / after.scale.z; rotationMatrix.m[2][1] = axisZ.y / after.scale.z; rotationMatrix.m[2][2] = axisZ.z / after.scale.z;
	after.rotate = Quaternion::FromMatrix(rotationMatrix).ToEuler();

	CommandHistory::GetInstance()->Execute(std::make_unique<GameObjectTransformCommand>(object->GetGuid(), before, after, gizmoDragId_));
}

///=============================================================================
///						操作
///=============================================================================

void SequencerEditor::AddTrack(const std::string& typeName)
{
	auto command = std::make_unique<SequenceStructureCommand>(&sequence_, "Add " + typeName + " Track");

	TrackPtr track = TrackFactory::Create(typeName);
	if (!track)
	{
		statusMessage_ = "未知のトラック種別です: " + typeName;
		return;
	}

	// 役の定義も同時に登録しておく（再生側が何を割り当てればよいか分かるように）
	BindingType bindingType;
	if (GetBindingTypeForTrack(*track, bindingType) && !track->GetBindingRole().empty())
	{
		BindingDefinition definition;
		definition.role = track->GetBindingRole();
		definition.type = bindingType;
		sequence_.AddBinding(definition);
	}

	sequence_.AddTrack(std::move(track));

	command->CaptureAfter();
	if (command->HasChanged())
	{
		CommandHistory::GetInstance()->Execute(std::move(command));
	}

	// 追加したトラックを選び、そのままインスペクタで役を割り当てられるようにする
	SelectionItem item;
	item.kind = SelectionKind::SequenceTrack;
	item.trackIndex = static_cast<int>(sequence_.GetTrackCount() - 1);
	SelectionContext::GetInstance()->Select(item);
	pendingScrollToTrack_ = item.trackIndex;
}

void SequencerEditor::AddKeyAtCurrentTime()
{
	// 対象トラックを決める。選択中のトラック（キー）があればそれ、無ければ先頭のトラック
	int trackIndex = GetSelectedTrackIndex();
	if (trackIndex < 0 && sequence_.GetTrackCount() > 0)
	{
		trackIndex = 0;
	}

	if (trackIndex < 0)
	{
		// トラックが1本も無い状態でキーを打とうとした場合は、カメラトラックを作る
		AddTrack("Camera");
		trackIndex = static_cast<int>(sequence_.GetTrackCount()) - 1;
		if (trackIndex < 0)
		{
			return;
		}
	}

	ITrack* track = sequence_.GetTrack(static_cast<size_t>(trackIndex));
	if (!track)
	{
		return;
	}

	bool recorded = false;
	const float time = SnapTime(player_.GetTime());
	const bool changed = ExecuteTrackEdit(sequence_, static_cast<size_t>(trackIndex), "Add Key",
		[&]() { recorded = track->RecordKey(time, player_.GetBindingContext()); });

	if (!recorded)
	{
		statusMessage_ = "キーを打てません: \"" + track->GetName() + "\" の対象が割り当てられていません";
		return;
	}

	if (changed)
	{
		player_.EvaluateCurrentTime();
		statusMessage_ = "キーを追加しました: " + track->GetName();
	}
}

void SequencerEditor::DeleteSelectedKey()
{
	std::vector<SelectionItem> keys = CollectSelectedKeys(sequence_);
	if (keys.empty())
	{
		return;
	}

	// トラックごとにまとめ、同じチャンネルの中は後ろのキーから消す。前から消すと残りの番号がずれる
	std::sort(keys.begin(), keys.end(), [](const SelectionItem& a, const SelectionItem& b)
	{
		if (a.trackIndex != b.trackIndex) { return a.trackIndex < b.trackIndex; }
		if (a.channelIndex != b.channelIndex) { return a.channelIndex < b.channelIndex; }
		return a.keyIndex > b.keyIndex;
	});

	CommandHistory* history = CommandHistory::GetInstance();
	history->BeginTransaction("Delete Keys");
	bool changed = false;
	for (size_t begin = 0; begin < keys.size(); )
	{
		size_t end = begin;
		while (end < keys.size() && keys[end].trackIndex == keys[begin].trackIndex)
		{
			++end;
		}
		ITrack* track = sequence_.GetTrack(static_cast<size_t>(keys[begin].trackIndex));
		changed |= ExecuteTrackEdit(sequence_, static_cast<size_t>(keys[begin].trackIndex), "Delete Keys", [&]()
		{
			for (size_t i = begin; i < end; ++i)
			{
				track->GetChannel(static_cast<size_t>(keys[i].channelIndex))->RemoveKey(static_cast<size_t>(keys[i].keyIndex));
			}
		});
		begin = end;
	}
	history->EndTransaction();

	if (changed)
	{
		SelectionContext::GetInstance()->ClearSelection();
		player_.EvaluateCurrentTime();
	}
}

void SequencerEditor::CopySelectedKeys()
{
	const std::vector<SelectionItem> keys = CollectSelectedKeys(sequence_);
	if (keys.empty())
	{
		return;
	}

	clipboard_.clear();
	for (const SelectionItem& item : keys)
	{
		ICurveChannel* channel = GetSelectedChannel(sequence_, item);
		nlohmann::json keyJson = channel->CopyKey(static_cast<size_t>(item.keyIndex));
		if (keyJson.is_null())
		{
			continue;
		}
		ClipboardKey entry;
		entry.trackIndex = item.trackIndex;
		entry.channelIndex = item.channelIndex;
		entry.trackType = sequence_.GetTrack(static_cast<size_t>(item.trackIndex))->GetTypeName();
		entry.channelName = channel->GetName();
		entry.offset = channel->GetKeyTime(static_cast<size_t>(item.keyIndex));
		entry.key = std::move(keyJson);
		clipboard_.push_back(std::move(entry));
	}
	if (clipboard_.empty())
	{
		statusMessage_ = "選んだキーはコピーに対応していません";
		return;
	}

	// 一番早いキーを 0 にして、貼り付けたときに再生位置から並ぶようにする
	float earliest = clipboard_.front().offset;
	for (const ClipboardKey& entry : clipboard_)
	{
		earliest = (std::min)(earliest, entry.offset);
	}
	for (ClipboardKey& entry : clipboard_)
	{
		entry.offset -= earliest;
	}
	statusMessage_ = std::to_string(clipboard_.size()) + " 個のキーをコピーしました";
}

void SequencerEditor::PasteKeysAtCurrentTime()
{
	PasteKeysAt(player_.GetTime());
}

void SequencerEditor::PasteKeysAt(float baseTime)
{
	if (clipboard_.empty())
	{
		return;
	}

	// 貼り付け先は、コピー元と同じ番号・同じ種類のトラックの同じチャンネル。トラックを消したり並べ替えたりしていたら貼らない
	const auto findChannel = [this](const ClipboardKey& entry) -> ICurveChannel*
	{
		if (entry.trackIndex < 0 || static_cast<size_t>(entry.trackIndex) >= sequence_.GetTrackCount())
		{
			return nullptr;
		}
		ITrack* track = sequence_.GetTrack(static_cast<size_t>(entry.trackIndex));
		if (!track || entry.trackType != track->GetTypeName())
		{
			return nullptr;
		}
		ICurveChannel* channel = entry.channelIndex >= 0 ? track->GetChannel(static_cast<size_t>(entry.channelIndex)) : nullptr;
		return channel && entry.channelName == channel->GetName() ? channel : nullptr;
	};

	CommandHistory* history = CommandHistory::GetInstance();
	history->BeginTransaction("Paste Keys");
	// トラックごとに1コマンドにする
	std::vector<int> pastedTracks;
	for (const ClipboardKey& entry : clipboard_)
	{
		if (std::find(pastedTracks.begin(), pastedTracks.end(), entry.trackIndex) != pastedTracks.end() || !findChannel(entry))
		{
			continue;
		}
		pastedTracks.push_back(entry.trackIndex);
		ExecuteTrackEdit(sequence_, static_cast<size_t>(entry.trackIndex), "Paste Keys", [&]()
		{
			for (const ClipboardKey& other : clipboard_)
			{
				if (other.trackIndex != entry.trackIndex)
				{
					continue;
				}
				if (ICurveChannel* channel = findChannel(other))
				{
					channel->PasteKey(baseTime + other.offset, other.key);
				}
			}
		});
	}
	history->EndTransaction();

	// 貼り付けたキーを選び直す。後から入れたキーで番号がずれるので、時刻で探す
	SelectionContext* selection = SelectionContext::GetInstance();
	selection->ClearSelection();
	size_t pastedCount = 0;
	for (const ClipboardKey& entry : clipboard_)
	{
		ICurveChannel* channel = findChannel(entry);
		const int keyIndex = channel ? channel->FindKeyAt(baseTime + entry.offset, kPastedKeyTolerance) : -1;
		if (keyIndex < 0)
		{
			continue;
		}
		SelectionItem item;
		item.kind = SelectionKind::SequenceKey;
		item.trackIndex = entry.trackIndex;
		item.channelIndex = entry.channelIndex;
		item.keyIndex = keyIndex;
		selection->AddToSelection(item);
		++pastedCount;
	}
	statusMessage_ = pastedCount > 0
		? std::to_string(pastedCount) + " 個のキーを貼り付けました"
		: std::string("貼り付け先のトラックが見つかりません（コピー元と同じトラックにだけ貼れます）");
	player_.EvaluateCurrentTime();
}

void SequencerEditor::DrawTimelineContextMenu()
{
	if (!ImGui::BeginPopup("TimelineContextMenu")) { return; }
	ITrack* track = contextTrackIndex_ >= 0 ? sequence_.GetTrack(static_cast<size_t>(contextTrackIndex_)) : nullptr;
	ICurveChannel* channel = track && contextChannelIndex_ >= 0
		? track->GetChannel(static_cast<size_t>(contextChannelIndex_)) : nullptr;
	if (channel && ImGui::MenuItem("ここにキーを打つ"))
	{
		bool added = false;
		ExecuteTrackEdit(sequence_, static_cast<size_t>(contextTrackIndex_), "Add Key", [&]() { added = channel->AddKeyAt(contextTime_); });
		statusMessage_ = added ? "キーを追加しました" : "この行にはキーを追加できません";
		player_.EvaluateCurrentTime();
	}
	if (ImGui::MenuItem("選択したキーを削除", nullptr, false, !CollectSelectedKeys(sequence_).empty())) { DeleteSelectedKey(); }
	if (ImGui::MenuItem("コピー", nullptr, false, !CollectSelectedKeys(sequence_).empty())) { CopySelectedKeys(); }
	if (ImGui::MenuItem("ここに貼り付け", nullptr, false, !clipboard_.empty())) { PasteKeysAt(contextTime_); }

	const std::vector<SelectionItem> selected = CollectSelectedKeys(sequence_);
	if (ImGui::BeginMenu("補間", !selected.empty()))
	{
		auto setInterpolation = [&](InterpolationMode mode)
		{
			CommandHistory* history = CommandHistory::GetInstance();
			history->BeginTransaction("Change Interpolation");
			for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount(); ++trackIndex)
			{
				ExecuteTrackEdit(sequence_, trackIndex, "Change Interpolation", [&]()
				{
					for (const SelectionItem& item : selected)
					{
						if (item.trackIndex != static_cast<int>(trackIndex)) { continue; }
						if (ICurveChannel* selectedChannel = GetSelectedChannel(sequence_, item))
						{
							if (selectedChannel->HasInterpolation())
							{
								selectedChannel->GetKeyInterp(static_cast<size_t>(item.keyIndex)) = mode;
							}
						}
					}
				});
			}
			history->EndTransaction();
		};
		if (ImGui::MenuItem("一定")) { setInterpolation(InterpolationMode::Constant); }
		if (ImGui::MenuItem("直線")) { setInterpolation(InterpolationMode::Linear); }
		if (ImGui::MenuItem("ベジェ")) { setInterpolation(InterpolationMode::Bezier); }
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("イージング", !selected.empty()))
	{
		struct Preset { const char* name; EasingType type; };
		const Preset presets[] = {
			{ "Linear", EasingType::Linear }, { "Ease In", EasingType::EaseInCubic },
			{ "Ease Out", EasingType::EaseOutCubic }, { "Ease In Out", EasingType::EaseInOutCubic },
		};
		for (const Preset& preset : presets)
		{
			if (!ImGui::MenuItem(preset.name)) { continue; }
			CommandHistory* history = CommandHistory::GetInstance();
			history->BeginTransaction("Set Easing Preset");
			for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount(); ++trackIndex)
			{
				ExecuteTrackEdit(sequence_, trackIndex, "Set Easing Preset", [&]()
				{
					for (const SelectionItem& item : selected)
					{
						if (item.trackIndex != static_cast<int>(trackIndex)) { continue; }
						if (ICurveChannel* selectedChannel = GetSelectedChannel(sequence_, item))
						{
							if (selectedChannel->HasInterpolation())
							{
								selectedChannel->GetKeyInterp(static_cast<size_t>(item.keyIndex)) = InterpolationMode::Bezier;
								selectedChannel->GetKeyBezier(static_cast<size_t>(item.keyIndex)) = EasingTypeToBezier(preset.type);
							}
						}
					}
				});
			}
			history->EndTransaction();
		}
		ImGui::EndMenu();
	}
	ImGui::EndPopup();
}

void SequencerEditor::ApplyActiveCamera()
{
	if (!cameraManager_)
	{
		return;
	}

	cameraManager_->SetActiveCamera(previewThroughSequenceCamera_ ? sequenceCameraName_ : editorCameraName_);
}

void SequencerEditor::UpdateEditorCameraFly()
{
	if (!cameraManager_ || previewThroughSequenceCamera_)
	{
		return;
	}

	Camera* camera = cameraManager_->GetCamera(editorCameraName_);
	if (!camera)
	{
		return;
	}

	// シーンビューの上で右ドラッグしている間だけ操作を受け付ける。
	// そうしないと、他のウィンドウを触っている間にカメラが動いてしまう。
	if (!SceneViewContext::HasInstance())
	{
		return;
	}

	const SceneViewRect& rect = SceneViewContext::GetInstance()->GetViewportRect();
	if (!rect.IsValid())
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	const bool mouseInScene =
		io.MousePos.x >= rect.x && io.MousePos.x <= rect.x + rect.width &&
		io.MousePos.y >= rect.y && io.MousePos.y <= rect.y + rect.height;

	if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || (!mouseInScene && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)))
	{
		return;
	}

	// 視点回転
	constexpr float kLookSensitivity = 0.003f;
	Vector3 rotate = camera->GetRotate();
	rotate.y += io.MouseDelta.x * kLookSensitivity;
	rotate.x += io.MouseDelta.y * kLookSensitivity;
	// 真上・真下で反転しないよう制限する
	rotate.x = std::clamp(rotate.x, -1.55f, 1.55f);
	camera->SetRotate(rotate);

	// ホイールで移動速度を変える
	if (io.MouseWheel != 0.0f)
	{
		editorCameraSpeed_ = std::clamp(editorCameraSpeed_ * (1.0f + io.MouseWheel * 0.1f), 0.1f, 200.0f);
	}

	// 移動。編集モードではゲーム時間が止まるため、実時間の差分を使う
	const float deltaTime = io.DeltaTime;
	const Matrix4x4 world = camera->GetWorldMatrix();
	const Vector3 forward = { world.m[2][0], world.m[2][1], world.m[2][2] };
	const Vector3 right = { world.m[0][0], world.m[0][1], world.m[0][2] };
	const Vector3 up = { world.m[1][0], world.m[1][1], world.m[1][2] };

	Vector3 move = { 0.0f, 0.0f, 0.0f };
	if (ImGui::IsKeyDown(ImGuiKey_W)) { move += forward; }
	if (ImGui::IsKeyDown(ImGuiKey_S)) { move -= forward; }
	if (ImGui::IsKeyDown(ImGuiKey_D)) { move += right; }
	if (ImGui::IsKeyDown(ImGuiKey_A)) { move -= right; }
	if (ImGui::IsKeyDown(ImGuiKey_E)) { move += up; }
	if (ImGui::IsKeyDown(ImGuiKey_Q)) { move -= up; }

	if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f)
	{
		const float speed = editorCameraSpeed_ * (io.KeyShift ? 3.0f : 1.0f);
		camera->SetTranslate(camera->GetTranslate() + move.Normalize() * speed * deltaTime);
	}
}

void SequencerEditor::HandleShortcuts()
{
	ImGuiIO& io = ImGui::GetIO();

	// テキスト入力中はショートカットを効かせない。
	// 名前の入力中に Space で再生が始まるようなことを防ぐ。
	if (io.WantTextInput)
	{
		return;
	}

	CommandHistory* history = CommandHistory::GetInstance();

	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
	{
		if (io.KeyShift) { history->Redo(); }
		else { history->Undo(); }
		player_.EvaluateCurrentTime();
	}
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
	{
		history->Redo();
		player_.EvaluateCurrentTime();
	}
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
	{
		if (sequence_.SaveToFile(filePath_))
		{
			statusMessage_ = "保存しました: " + filePath_;
			history->MarkSaved();
		}
	}
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
	{
		CopySelectedKeys();
	}
	else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
	{
		PasteKeysAtCurrentTime();
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		if (player_.IsPlaying()) { player_.Pause(); }
		else { player_.Resume(); }
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_K, false))
	{
		AddKeyAtCurrentTime();
	}
	else if (timelineHovered_ && ImGui::IsKeyPressed(ImGuiKey_F, false))
	{
		FrameAllKeys();
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		DeleteSelectedKey();
	}
}
} // namespace KCE

#endif // USE_IMGUI
