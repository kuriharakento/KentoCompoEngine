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
#include "editor/EditorContext.h"
#include "editor/SceneViewContext.h"
#include "editor/SelectionContext.h"
#include "editor/command/CommandHistory.h"
#include "manager/editor/DebugUIManager.h"
#include "sequencer/editor/SequencerCommands.h"
#include "sequencer/track/CameraTrack.h"

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

/** @brief カメラトラックのチャンネル数（位置・回転・画角） */
constexpr int kCameraChannelCount = 3;
/** @brief チャンネルの表示名 */
const char* const kCameraChannelNames[kCameraChannelCount] = { "Position", "Rotation", "FOV" };

/** @brief キーのマーカーの半径（ピクセル） */
constexpr float kKeyMarkerRadius = 5.0f;
/** @brief キーを掴めるとみなす距離（ピクセル） */
constexpr float kKeyGrabRadius = 7.0f;
/** @brief 削除時に同一時刻とみなす許容差（秒） */
constexpr float kKeyDeleteTolerance = 0.005f;

/**
 * @brief チャンネルのキー数を返す
 */
size_t GetChannelKeyCount(const CameraTrack& track, int channel)
{
	switch (channel)
	{
	case 0:  return track.GetPositionCurve().GetKeyCount();
	case 1:  return track.GetRotationCurve().GetKeyCount();
	case 2:  return track.GetFovCurve().GetKeyCount();
	default: return 0;
	}
}

/**
 * @brief チャンネル内のキーの時刻を返す
 */
float GetChannelKeyTime(const CameraTrack& track, int channel, int keyIndex)
{
	const size_t index = static_cast<size_t>(keyIndex);
	if (keyIndex < 0 || index >= GetChannelKeyCount(track, channel))
	{
		return 0.0f;
	}

	switch (channel)
	{
	case 0:  return track.GetPositionCurve().GetKey(index).time;
	case 1:  return track.GetRotationCurve().GetKey(index).time;
	case 2:  return track.GetFovCurve().GetKey(index).time;
	default: return 0.0f;
	}
}

/**
 * @brief チャンネル内のキーの時刻を変更する
 * @return 移動後のキーのインデックス
 */
int SetChannelKeyTime(CameraTrack& track, int channel, int keyIndex, float newTime)
{
	const size_t index = static_cast<size_t>(keyIndex);
	if (keyIndex < 0 || index >= GetChannelKeyCount(track, channel))
	{
		return keyIndex;
	}

	switch (channel)
	{
	case 0:  return static_cast<int>(track.GetPositionCurve().MoveKey(index, newTime));
	case 1:  return static_cast<int>(track.GetRotationCurve().MoveKey(index, newTime));
	case 2:  return static_cast<int>(track.GetFovCurve().MoveKey(index, newTime));
	default: return keyIndex;
	}
}

/**
 * @brief チャンネル内のキーのベジェ制御点への参照を返す
 * @return 参照。範囲外なら nullptr
 */
BezierHandle* GetChannelKeyBezier(CameraTrack& track, int channel, int keyIndex)
{
	const size_t index = static_cast<size_t>(keyIndex);
	if (keyIndex < 0 || index >= GetChannelKeyCount(track, channel))
	{
		return nullptr;
	}

	switch (channel)
	{
	case 0:  return &track.GetPositionCurve().GetKey(index).bezier;
	case 1:  return &track.GetRotationCurve().GetKey(index).bezier;
	case 2:  return &track.GetFovCurve().GetKey(index).bezier;
	default: return nullptr;
	}
}

/**
 * @brief チャンネル内のキーの補間モードへの参照を返す
 * @return 参照。範囲外なら nullptr
 */
InterpolationMode* GetChannelKeyInterp(CameraTrack& track, int channel, int keyIndex)
{
	const size_t index = static_cast<size_t>(keyIndex);
	if (keyIndex < 0 || index >= GetChannelKeyCount(track, channel))
	{
		return nullptr;
	}

	switch (channel)
	{
	case 0:  return &track.GetPositionCurve().GetKey(index).interp;
	case 1:  return &track.GetRotationCurve().GetKey(index).interp;
	case 2:  return &track.GetFovCurve().GetKey(index).interp;
	default: return nullptr;
	}
}

/**
 * @brief 現在選択されているキーを取得する
 * @return 選択がキーでなければ kind == None のアイテム
 */
const SelectionItem& GetSelectedKey()
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
} // namespace

void SequencerEditor::Initialize(CameraManager* cameraManager)
{
	if (initialized_)
	{
		return;
	}

	cameraManager_ = cameraManager;
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

	// シーケンスは必ず MainCam の役を要求する
	BindingDefinition binding;
	binding.role = kCameraRole;
	binding.type = BindingType::Camera;
	binding.description = "シーケンスが動かすカメラ";
	sequence_.AddBinding(binding);

	player_.SetSequence(&sequence_);

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
	cameraManager_ = nullptr;
	initialized_ = false;
	instance_.reset();
}

void SequencerEditor::SetSequenceCamera(Camera* camera)
{
	player_.GetBindingContext().BindCamera(kCameraRole, camera);
}

Camera* SequencerEditor::GetSequenceCamera() const
{
	return player_.GetBindingContext().GetCamera(kCameraRole);
}

void SequencerEditor::Update()
{
	if (!initialized_)
	{
		return;
	}

	HandleShortcuts();
	UpdateEditorCameraFly();
	player_.Update();
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
	if (ImGui::Button("Add Camera Track"))
	{
		AddCameraTrack();
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
	ImGui::Combo("Gizmo", &gizmoOperation_, "Translate\0Rotate\0");
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

	float rowY = canvasMin.y + view_.rulerHeight;

	for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount(); ++trackIndex)
	{
		ITrack* track = sequence_.GetTrack(trackIndex);
		if (!track)
		{
			continue;
		}

		auto* cameraTrack = dynamic_cast<CameraTrack*>(track);
		const int channelCount = cameraTrack ? kCameraChannelCount : 1;

		for (int channel = 0; channel < channelCount; ++channel)
		{
			const float rowTop = rowY;
			const float rowBottom = rowY + view_.trackHeight;
			const float rowCenter = (rowTop + rowBottom) * 0.5f;

			// 行の背景。1行おきに明度を変えて追いやすくする
			const bool isEvenRow = ((trackIndex * kCameraChannelCount + channel) % 2) == 0;
			drawList->AddRectFilled(
				ImVec2(canvasMin.x, rowTop),
				ImVec2(canvasMin.x + canvasSize.x, rowBottom),
				isEvenRow ? IM_COL32(30, 30, 34, 255) : IM_COL32(34, 34, 39, 255));

			// トラック名欄
			char headerLabel[128];
			if (cameraTrack)
			{
				std::snprintf(headerLabel, sizeof(headerLabel), "%s / %s", track->GetName().c_str(), kCameraChannelNames[channel]);
			}
			else
			{
				std::snprintf(headerLabel, sizeof(headerLabel), "%s", track->GetName().c_str());
			}

			drawList->AddText(
				ImVec2(canvasMin.x + 6.0f, rowCenter - ImGui::GetTextLineHeight() * 0.5f),
				track->IsMuted() ? IM_COL32(110, 110, 115, 255) : IM_COL32(215, 215, 225, 255),
				headerLabel);

			// ミュートの切り替えボタン。チャンネル0の行にだけ置く
			if (channel == 0)
			{
				ImGui::SetCursorScreenPos(ImVec2(canvasMin.x + view_.headerWidth - 30.0f, rowTop + 2.0f));
				ImGui::PushID(static_cast<int>(trackIndex));
				if (ImGui::SmallButton(track->IsMuted() ? "M" : "-"))
				{
					track->SetMuted(!track->IsMuted());
					player_.EvaluateCurrentTime();
				}
				ImGui::PopID();
			}

			if (!cameraTrack)
			{
				rowY = rowBottom;
				continue;
			}

			// キーの描画とヒット判定
			const size_t keyCount = GetChannelKeyCount(*cameraTrack, channel);
			for (size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
			{
				const float keyTime = GetChannelKeyTime(*cameraTrack, channel, static_cast<int>(keyIndex));
				const float x = TimeToPixel(keyTime, left);
				if (x < left - kKeyMarkerRadius || x > canvasMin.x + canvasSize.x + kKeyMarkerRadius)
				{
					continue;
				}

				SelectionItem item;
				item.kind = SelectionKind::SequenceKey;
				item.trackIndex = static_cast<int>(trackIndex);
				item.channelIndex = channel;
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

	// --- キーの選択とドラッグ ---
	const ImVec2 mousePos = ImGui::GetIO().MousePos;
	const bool mouseInCanvas =
		mousePos.x >= left && mousePos.x <= canvasMin.x + canvasSize.x &&
		mousePos.y >= canvasMin.y + view_.rulerHeight && mousePos.y <= canvasMin.y + canvasSize.y;

	if (mouseInCanvas && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !draggingPlayhead_)
	{
		// クリック位置に最も近いキーを拾う
		float bestDistance = kKeyGrabRadius;
		SelectionItem bestItem;

		float searchRowY = canvasMin.y + view_.rulerHeight;
		for (size_t trackIndex = 0; trackIndex < sequence_.GetTrackCount(); ++trackIndex)
		{
			auto* cameraTrack = dynamic_cast<CameraTrack*>(sequence_.GetTrack(trackIndex));
			const int channelCount = cameraTrack ? kCameraChannelCount : 1;

			for (int channel = 0; channel < channelCount; ++channel)
			{
				const float rowCenter = searchRowY + view_.trackHeight * 0.5f;
				searchRowY += view_.trackHeight;

				if (!cameraTrack || std::abs(mousePos.y - rowCenter) > view_.trackHeight * 0.5f)
				{
					continue;
				}

				const size_t keyCount = GetChannelKeyCount(*cameraTrack, channel);
				for (size_t keyIndex = 0; keyIndex < keyCount; ++keyIndex)
				{
					const float keyTime = GetChannelKeyTime(*cameraTrack, channel, static_cast<int>(keyIndex));
					const float distance = std::abs(TimeToPixel(keyTime, left) - mousePos.x);
					if (distance < bestDistance)
					{
						bestDistance = distance;
						bestItem.kind = SelectionKind::SequenceKey;
						bestItem.trackIndex = static_cast<int>(trackIndex);
						bestItem.channelIndex = channel;
						bestItem.keyIndex = static_cast<int>(keyIndex);
					}
				}
			}
		}

		if (bestItem.kind == SelectionKind::SequenceKey)
		{
			selection->Select(bestItem);
			draggingKey_ = true;
		}
		else
		{
			selection->ClearSelection();
		}
	}

	if (draggingKey_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const SelectionItem& selected = GetSelectedKey();
		if (selected.kind == SelectionKind::SequenceKey)
		{
			auto* cameraTrack = dynamic_cast<CameraTrack*>(sequence_.GetTrack(static_cast<size_t>(selected.trackIndex)));
			if (cameraTrack)
			{
				const float newTime = (std::max)(SnapTime(PixelToTime(mousePos.x, left)), 0.0f);

				// ドラッグ中は毎フレームコマンドを発行し、MergeWith で1つにまとめる。
				// 履歴が1ドラッグ1件になり、Undo1回で掴む前の位置に戻る。
				auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Move Key");
				const int newIndex = SetChannelKeyTime(*cameraTrack, selected.channelIndex, selected.keyIndex, newTime);
				command->CaptureAfter();

				if (command->HasChanged())
				{
					CommandHistory::GetInstance()->Execute(std::move(command));

					// 時刻の変更で並び順が変わりうるので、選択のインデックスを追従させる
					SelectionItem updated = selected;
					updated.keyIndex = newIndex;
					selection->Select(updated);

					player_.EvaluateCurrentTime();
				}
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
	ImGui::Dummy(canvasSize);
}

///=============================================================================
///						インスペクタ
///=============================================================================

void SequencerEditor::DrawBezierEditor()
{
	const SelectionItem& selected = GetSelectedKey();
	if (selected.kind != SelectionKind::SequenceKey)
	{
		return;
	}

	auto* cameraTrack = dynamic_cast<CameraTrack*>(sequence_.GetTrack(static_cast<size_t>(selected.trackIndex)));
	if (!cameraTrack)
	{
		return;
	}

	InterpolationMode* interp = GetChannelKeyInterp(*cameraTrack, selected.channelIndex, selected.keyIndex);
	BezierHandle* bezier = GetChannelKeyBezier(*cameraTrack, selected.channelIndex, selected.keyIndex);
	if (!interp || !bezier)
	{
		return;
	}

	ImGui::SeparatorText("Interpolation (このキーから次のキューまで)");

	int interpIndex = static_cast<int>(*interp);
	if (ImGui::Combo("Mode", &interpIndex, "Constant\0Linear\0Bezier\0"))
	{
		auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Change Interpolation");
		*interp = static_cast<InterpolationMode>(interpIndex);
		command->CaptureAfter();
		if (command->HasChanged())
		{
			CommandHistory::GetInstance()->Execute(std::move(command));
			player_.EvaluateCurrentTime();
		}
	}

	if (*interp != InterpolationMode::Bezier)
	{
		return;
	}

	// プリセット
	if (ImGui::Button("Ease In-Out"))
	{
		auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Set Easing Preset");
		*bezier = EasingTypeToBezier(EasingType::EaseInOutCubic);
		command->CaptureAfter();
		if (command->HasChanged())
		{
			CommandHistory::GetInstance()->Execute(std::move(command));
			player_.EvaluateCurrentTime();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Ease Out"))
	{
		auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Set Easing Preset");
		*bezier = EasingTypeToBezier(EasingType::EaseOutCubic);
		command->CaptureAfter();
		if (command->HasChanged())
		{
			CommandHistory::GetInstance()->Execute(std::move(command));
			player_.EvaluateCurrentTime();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Linear"))
	{
		auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Set Easing Preset");
		*bezier = EasingTypeToBezier(EasingType::Linear);
		command->CaptureAfter();
		if (command->HasChanged())
		{
			CommandHistory::GetInstance()->Execute(std::move(command));
			player_.EvaluateCurrentTime();
		}
	}

	// 数値でのハンドル編集
	float handle[4] = { bezier->x1, bezier->y1, bezier->x2, bezier->y2 };
	if (ImGui::DragFloat4("cubic-bezier", handle, 0.01f, -2.0f, 3.0f, "%.3f"))
	{
		auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Edit Bezier Handle");
		// x は単調でなければ解が一意に定まらないため 0〜1 に制限する
		bezier->x1 = std::clamp(handle[0], 0.0f, 1.0f);
		bezier->y1 = handle[1];
		bezier->x2 = std::clamp(handle[2], 0.0f, 1.0f);
		bezier->y2 = handle[3];
		command->CaptureAfter();
		if (command->HasChanged())
		{
			CommandHistory::GetInstance()->Execute(std::move(command));
			player_.EvaluateCurrentTime();
		}
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
		const float value = ApplyBezierEasing(*bezier, t);
		const ImVec2 point(graphMin.x + t * graphSize, graphMin.y + graphSize - value * graphSize);
		drawList->AddLine(previousPoint, point, IM_COL32(120, 200, 255, 255), 1.5f);
		previousPoint = point;
	}
	ImGui::Dummy(ImVec2(graphSize, graphSize));
}

void SequencerEditor::DrawInspectorWindow()
{
	const SelectionItem& selected = GetSelectedKey();
	if (selected.kind != SelectionKind::SequenceKey)
	{
		ImGui::TextDisabled("タイムラインでキーを選択してください");
		return;
	}

	auto* cameraTrack = dynamic_cast<CameraTrack*>(sequence_.GetTrack(static_cast<size_t>(selected.trackIndex)));
	if (!cameraTrack)
	{
		ImGui::TextDisabled("選択中のトラックが見つかりません");
		return;
	}

	if (selected.channelIndex < 0 || selected.channelIndex >= kCameraChannelCount)
	{
		return;
	}

	ImGui::Text("Track: %s", cameraTrack->GetName().c_str());
	ImGui::Text("Channel: %s", kCameraChannelNames[selected.channelIndex]);
	ImGui::Text("Key: %d", selected.keyIndex);

	float keyTime = GetChannelKeyTime(*cameraTrack, selected.channelIndex, selected.keyIndex);
	if (ImGui::DragFloat("Time", &keyTime, 0.01f, 0.0f, 3600.0f, "%.3f s"))
	{
		auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Set Key Time");
		const int newIndex = SetChannelKeyTime(*cameraTrack, selected.channelIndex, selected.keyIndex, keyTime);
		command->CaptureAfter();
		if (command->HasChanged())
		{
			CommandHistory::GetInstance()->Execute(std::move(command));
			SelectionItem updated = selected;
			updated.keyIndex = newIndex;
			SelectionContext::GetInstance()->Select(updated);
			player_.EvaluateCurrentTime();
		}
	}

	// チャンネルごとの値の編集
	const size_t index = static_cast<size_t>(selected.keyIndex);
	if (selected.channelIndex == 0 && index < cameraTrack->GetPositionCurve().GetKeyCount())
	{
		Vector3& value = cameraTrack->GetPositionCurve().GetKey(index).value;
		float components[3] = { value.x, value.y, value.z };
		if (ImGui::DragFloat3("Position", components, 0.05f))
		{
			auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Edit Key Value");
			value = { components[0], components[1], components[2] };
			command->CaptureAfter();
			if (command->HasChanged())
			{
				CommandHistory::GetInstance()->Execute(std::move(command));
				player_.EvaluateCurrentTime();
			}
		}
	}
	else if (selected.channelIndex == 1 && index < cameraTrack->GetRotationCurve().GetKeyCount())
	{
		Quaternion& value = cameraTrack->GetRotationCurve().GetKey(index).value;
		// 入力はオイラー角で受けるが、保持と補間はクォータニオンのまま行う
		Vector3 euler = value.ToEuler();
		float components[3] = { euler.x, euler.y, euler.z };
		if (ImGui::DragFloat3("Rotation (rad)", components, 0.01f))
		{
			auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Edit Key Value");
			value = Quaternion::FromEuler({ components[0], components[1], components[2] });
			command->CaptureAfter();
			if (command->HasChanged())
			{
				CommandHistory::GetInstance()->Execute(std::move(command));
				player_.EvaluateCurrentTime();
			}
		}
	}
	else if (selected.channelIndex == 2 && index < cameraTrack->GetFovCurve().GetKeyCount())
	{
		float& value = cameraTrack->GetFovCurve().GetKey(index).value;
		float fov = value;
		if (ImGui::DragFloat("FOV (rad)", &fov, 0.005f, 0.05f, 3.0f, "%.3f"))
		{
			auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Edit Key Value");
			value = fov;
			command->CaptureAfter();
			if (command->HasChanged())
			{
				CommandHistory::GetInstance()->Execute(std::move(command));
				player_.EvaluateCurrentTime();
			}
		}
	}

	DrawBezierEditor();
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

	if (!rect.IsValid() || !viewCamera || !targetCamera)
	{
		return;
	}

	// シーケンスカメラ視点で見ているときは、自分自身をギズモで動かすことになり
	// 操作が成立しないので描かない
	if (previewThroughSequenceCamera_ || viewCamera == targetCamera)
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
	Matrix4x4 world = targetCamera->GetWorldMatrix();

	const ImGuizmo::OPERATION operation = (gizmoOperation_ == 0) ? ImGuizmo::TRANSLATE : ImGuizmo::ROTATE;
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

///=============================================================================
///						操作
///=============================================================================

void SequencerEditor::AddCameraTrack()
{
	auto command = std::make_unique<SequenceStructureCommand>(&sequence_, "Add Camera Track");

	auto track = std::make_unique<CameraTrack>();
	track->SetBindingRole(kCameraRole);
	sequence_.AddTrack(std::move(track));

	command->CaptureAfter();
	if (command->HasChanged())
	{
		CommandHistory::GetInstance()->Execute(std::move(command));
	}
}

void SequencerEditor::AddKeyAtCurrentTime()
{
	Camera* camera = GetSequenceCamera();
	if (!camera)
	{
		statusMessage_ = "シーケンスカメラが割り当てられていません";
		return;
	}

	// 対象トラックを決める。選択中のキーがあればそのトラック、無ければ最初のカメラトラック。
	size_t targetIndex = 0;
	CameraTrack* targetTrack = nullptr;

	const SelectionItem& selected = GetSelectedKey();
	if (selected.kind == SelectionKind::SequenceKey && selected.trackIndex >= 0)
	{
		targetIndex = static_cast<size_t>(selected.trackIndex);
		targetTrack = dynamic_cast<CameraTrack*>(sequence_.GetTrack(targetIndex));
	}

	if (!targetTrack)
	{
		for (size_t i = 0; i < sequence_.GetTrackCount(); ++i)
		{
			if (auto* track = dynamic_cast<CameraTrack*>(sequence_.GetTrack(i)))
			{
				targetIndex = i;
				targetTrack = track;
				break;
			}
		}
	}

	if (!targetTrack)
	{
		// トラックが1本も無い状態でキーを打とうとした場合は、先に作ってしまう
		AddCameraTrack();
		targetIndex = sequence_.GetTrackCount() - 1;
		targetTrack = dynamic_cast<CameraTrack*>(sequence_.GetTrack(targetIndex));
		if (!targetTrack)
		{
			return;
		}
	}

	auto command = std::make_unique<TrackEditCommand>(&sequence_, targetIndex, "Add Camera Key");
	targetTrack->AddKeyFromCamera(SnapTime(player_.GetTime()), camera);
	command->CaptureAfter();

	if (command->HasChanged())
	{
		CommandHistory::GetInstance()->Execute(std::move(command));
		player_.EvaluateCurrentTime();
		statusMessage_ = "キーを追加しました";
	}
}

void SequencerEditor::DeleteSelectedKey()
{
	const SelectionItem& selected = GetSelectedKey();
	if (selected.kind != SelectionKind::SequenceKey)
	{
		return;
	}

	auto* cameraTrack = dynamic_cast<CameraTrack*>(sequence_.GetTrack(static_cast<size_t>(selected.trackIndex)));
	if (!cameraTrack)
	{
		return;
	}

	const float keyTime = GetChannelKeyTime(*cameraTrack, selected.channelIndex, selected.keyIndex);

	auto command = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(selected.trackIndex), "Delete Key");
	cameraTrack->RemoveKeysAt(keyTime, kKeyDeleteTolerance);
	command->CaptureAfter();

	if (command->HasChanged())
	{
		CommandHistory::GetInstance()->Execute(std::move(command));
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
