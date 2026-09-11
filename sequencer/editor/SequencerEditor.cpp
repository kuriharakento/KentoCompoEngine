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

/** @brief キーのマーカーの半径（ピクセル） */
constexpr float kKeyMarkerRadius = 5.0f;
/** @brief キーを掴めるとみなす距離（ピクセル） */
constexpr float kKeyGrabRadius = 7.0f;

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
	default:                   return false;
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
	debugUI->RegisterDebugUI(this, "Sequencer", [this]() { DrawTimelineWindow(); }, DebugUIArea::Project);
	debugUI->RegisterDebugUI(this, "Sequencer Inspector", [this]() { DrawInspectorWindow(); }, DebugUIArea::Inspector);
	debugUI->RegisterDebugUI(this, "Sequencer Gizmo", [this]() { DrawSceneOverlay(); }, DebugUIArea::Scene);

	initialized_ = true;
}

void SequencerEditor::Finalize()
{
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
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
		ImGui::TextDisabled("%s", statusMessage_.c_str());
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
			const float firstBeatIndex = std::floor((startTime - meta.offset) / beatDuration);
			for (float beat = firstBeatIndex; ; beat += 1.0f)
			{
				const float t = meta.offset + beat * beatDuration;
				if (t > endTime) { break; }

				const float x = TimeToPixel(t, left);
				if (x < left) { continue; }

				// 4拍ごとに濃くして小節の頭が分かるようにする
				const bool isBarStart = std::fmod(std::abs(beat), 4.0f) < 0.001f;
				drawList->AddLine(
					ImVec2(x, rulerBottom),
					ImVec2(x, canvasMin.y + ImGui::GetContentRegionAvail().y),
					isBarStart ? IM_COL32(90, 90, 110, 160) : IM_COL32(70, 70, 80, 90));
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

void SequencerEditor::DrawTracks(const ImVec2& canvasMin, const ImVec2& canvasSize)
{
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	SelectionContext* selection = SelectionContext::GetInstance();
	const float left = canvasMin.x + view_.headerWidth;
	const int selectedTrack = GetSelectedTrackIndex();

	float rowY = canvasMin.y + view_.rulerHeight;
	size_t globalRow = 0;

	for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount(); ++trackIndex)
	{
		ITrack* track = sequence_.GetTrack(trackIndex);
		if (!track)
		{
			continue;
		}

		const size_t rowCount = GetRowCount(track);
		for (size_t row = 0; row < rowCount; ++row, ++globalRow)
		{
			const float rowTop = rowY;
			const float rowBottom = rowY + view_.trackHeight;
			const float rowCenter = (rowTop + rowBottom) * 0.5f;
			ICurveChannel* channel = track->GetChannel(row);

			// 行の背景。選択中のトラックは明るくし、それ以外は1行おきに明度を変える
			ImU32 background = (globalRow % 2 == 0) ? IM_COL32(30, 30, 34, 255) : IM_COL32(34, 34, 39, 255);
			if (static_cast<int>(trackIndex) == selectedTrack)
			{
				background = IM_COL32(44, 48, 62, 255);
			}
			drawList->AddRectFilled(ImVec2(canvasMin.x, rowTop), ImVec2(canvasMin.x + canvasSize.x, rowBottom), background);

			// トラック名欄
			char headerLabel[128];
			if (channel)
			{
				std::snprintf(headerLabel, sizeof(headerLabel), "%s / %s", track->GetName().c_str(), channel->GetName());
			}
			else
			{
				std::snprintf(headerLabel, sizeof(headerLabel), "%s", track->GetName().c_str());
			}
			drawList->AddText(
				ImVec2(canvasMin.x + 6.0f, rowCenter - ImGui::GetTextLineHeight() * 0.5f),
				track->IsMuted() ? IM_COL32(110, 110, 115, 255) : IM_COL32(215, 215, 225, 255),
				headerLabel);

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
				item.channelIndex = static_cast<int>(row);
				item.keyIndex = static_cast<int>(keyIndex);

				const bool isSelected = selection->IsSelected(item);

				// ひし形で描く。丸より「キーフレーム」に見える
				const ImVec2 points[4] = {
					ImVec2(x, rowCenter - kKeyMarkerRadius),
					ImVec2(x + kKeyMarkerRadius, rowCenter),
					ImVec2(x, rowCenter + kKeyMarkerRadius),
					ImVec2(x - kKeyMarkerRadius, rowCenter),
				};
				drawList->AddConvexPolyFilled(points, 4, isSelected ? IM_COL32(255, 200, 80, 255) : IM_COL32(150, 190, 255, 255));
				drawList->AddPolyline(points, 4, IM_COL32(20, 20, 25, 255), ImDrawFlags_Closed, 1.0f);
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

	// ミュートボタンなど他のウィジェットを押した場合は選択を変えない
	if (mouseInRows && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !draggingPlayhead_ && !ImGui::IsAnyItemHovered())
	{
		// クリックした行がどのトラックのどのチャンネルかを求める
		int hitTrack = -1;
		int hitChannel = -1;
		float searchRowY = rowsTop;
		for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount() && hitTrack < 0; ++trackIndex)
		{
			ITrack* track = sequence_.GetTrack(trackIndex);
			if (!track) { continue; }
			for (size_t row = 0; row < GetRowCount(track); ++row)
			{
				if (mousePos.y >= searchRowY && mousePos.y < searchRowY + view_.trackHeight)
				{
					hitTrack = static_cast<int>(trackIndex);
					hitChannel = static_cast<int>(row);
					break;
				}
				searchRowY += view_.trackHeight;
			}
		}

		SelectionItem picked;
		if (hitTrack >= 0 && !mouseInHeader)
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

		if (picked.kind == SelectionKind::SequenceKey)
		{
			selection->Select(picked);
			draggingKey_ = true;
		}
		else if (hitTrack >= 0)
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

	// --- キーのドラッグ ---
	if (draggingKey_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const SelectionItem selected = GetPrimarySelection();
		if (ICurveChannel* channel = GetSelectedChannel(sequence_, selected))
		{
			const float newTime = (std::max)(SnapTime(PixelToTime(mousePos.x, left)), 0.0f);

			// ドラッグ中は毎フレームコマンドを発行し、MergeWith で1つにまとめる。
			// 履歴が1ドラッグ1件になり、Undo1回で掴む前の位置に戻る。
			size_t newIndex = static_cast<size_t>(selected.keyIndex);
			const bool changed = ExecuteTrackEdit(sequence_, static_cast<size_t>(selected.trackIndex), "Move Key",
				[&]() { newIndex = channel->MoveKey(static_cast<size_t>(selected.keyIndex), newTime); });

			if (changed)
			{
				// 時刻の変更で並び順が変わりうるので、選択のインデックスを追従させる
				SelectionItem updated = selected;
				updated.keyIndex = static_cast<int>(newIndex);
				selection->Select(updated);
				player_.EvaluateCurrentTime();
			}
		}
	}

	if (draggingKey_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		draggingKey_ = false;
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

		char audioBuffer[128];
		std::snprintf(audioBuffer, sizeof(audioBuffer), "%s", meta.audioClip.c_str());
		if (ImGui::InputText("Audio Clip", audioBuffer, sizeof(audioBuffer)))
		{
			meta.audioClip = audioBuffer;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Audio に読み込み済みの音声名。\n設定するとこの音声の再生位置が時刻の権威になります");
		}

		ImGui::DragFloat("BPM", &meta.bpm, 0.1f, 0.0f, 400.0f, "%.2f");
		ImGui::DragFloat("Offset", &meta.offset, 0.001f, -10.0f, 10.0f, "%.3f s");
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

	// トラック名欄との境界
	drawList->AddLine(
		ImVec2(canvasMin.x + view_.headerWidth, canvasMin.y),
		ImVec2(canvasMin.x + view_.headerWidth, canvasMin.y + canvasSize.y),
		IM_COL32(70, 70, 80, 255));

	// このウィンドウ内のマウスホイールでズーム・スクロールする
	if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
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
		else
		{
			view_.scrollTime -= wheel * (100.0f / view_.pixelsPerSecond);
		}
		view_.scrollTime = (std::max)(view_.scrollTime, 0.0f);
	}

	DrawRuler(canvasMin, canvasSize.x);
	DrawTracks(canvasMin, canvasSize);
	DrawPlayhead(canvasMin, canvasSize);

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

	// カーブのプレビュー
	const ImVec2 graphMin = ImGui::GetCursorScreenPos();
	const float graphSize = 160.0f;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(graphMin, ImVec2(graphMin.x + graphSize, graphMin.y + graphSize), IM_COL32(20, 20, 24, 255));
	drawList->AddRect(graphMin, ImVec2(graphMin.x + graphSize, graphMin.y + graphSize), IM_COL32(80, 80, 90, 255));

	constexpr int kPreviewSegments = 48;
	ImVec2 previousPoint(graphMin.x, graphMin.y + graphSize);
	for (int i = 1; i <= kPreviewSegments; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(kPreviewSegments);
		const float value = ApplyBezierEasing(bezier, t);
		const ImVec2 point(graphMin.x + t * graphSize, graphMin.y + graphSize - value * graphSize);
		drawList->AddLine(previousPoint, point, IM_COL32(120, 200, 255, 255), 1.5f);
		previousPoint = point;
	}
	ImGui::Dummy(ImVec2(graphSize, graphSize));
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
		ExecuteTrackEdit(sequence_, trackIndex, "Edit Track Settings", [&]() { changed = track->DrawInspector(); });
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
	{
		const auto it = previewObjectBindings_.find(role);
		GameObject* current = (it != previewObjectBindings_.end() && GameObjectManager::HasInstance())
			? GameObjectManager::GetInstance()->FindByGuid(it->second)
			: nullptr;

		if (ImGui::BeginCombo("Preview Object", current ? current->GetName().c_str() : "(未割り当て)"))
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
		break;
	}

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
	const SelectionItem selected = GetPrimarySelection();
	ICurveChannel* channel = GetSelectedChannel(sequence_, selected);
	if (!channel)
	{
		return;
	}

	if (ExecuteTrackEdit(sequence_, static_cast<size_t>(selected.trackIndex), "Delete Key",
		[&]() { channel->RemoveKey(static_cast<size_t>(selected.keyIndex)); }))
	{
		SelectionContext::GetInstance()->ClearSelection();
		player_.EvaluateCurrentTime();
	}
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
	else if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		if (player_.IsPlaying()) { player_.Pause(); }
		else { player_.Resume(); }
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_K, false))
	{
		AddKeyAtCurrentTime();
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		DeleteSelectedKey();
	}
}
} // namespace KCE

#endif // USE_IMGUI
