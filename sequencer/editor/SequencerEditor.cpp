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
#include <filesystem>

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
	/**
	 * @param bindings 書き換える割り当て
	 * @param role 役
	 * @param after 割り当てる GameObject の GUID
	 * @param unbind 真なら after を使わず、割り当てを外す
	 */
	PreviewObjectBindingCommand(std::unordered_map<std::string, Guid>* bindings, std::string role, Guid after, bool unbind = false)
		: bindings_(bindings), role_(std::move(role)), after_(after), unbind_(unbind)
	{
		const auto it = bindings_->find(role_);
		if (it != bindings_->end())
		{
			before_ = it->second;
			hadBefore_ = true;
		}
	}

	void Execute() override
	{
		if (unbind_) { bindings_->erase(role_); }
		else { (*bindings_)[role_] = after_; }
	}
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
	bool unbind_ = false;
};

/** @brief シーケンスのエディタ用データで、プレビュー用の割り当てを置くキー */
const char* const kPreviewBindingsKey = "previewBindings";
/** @brief プレビュー用の割り当てのうち、GameObject（役 → 名前）のキー */
const char* const kPreviewObjectsKey = "objects";
/** @brief プレビュー用の割り当てのうち、ライト（役 → ライト名）のキー */
const char* const kPreviewLightsKey = "lights";

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
/** @brief 保存していない変更を捨てるかを聞く小窓の ID */
const char* const kDiscardChangesPopupId = "保存していない変更###DiscardSequenceChanges";
/** @brief 時間の目盛りの間隔の候補（秒）。ルーラーとカーブの横軸で共通 */
constexpr float kTimeTickSteps[] = { 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 30.0f, 60.0f };
/** @brief 時間の目盛りどうしに最低限あける間隔（ピクセル） */
constexpr float kTimeTickMinSpacing = 40.0f;
/** @brief カーブの縦軸（値）の目盛りの間隔の候補 */
constexpr float kValueTickSteps[] = { 0.01f, 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f };
/** @brief 値の目盛りどうしに最低限あける間隔（ピクセル） */
constexpr float kValueTickMinSpacing = 30.0f;
/** @brief 目盛りの数字と線のすき間（ピクセル） */
constexpr float kAxisLabelPadding = 2.0f;
/** @brief チャンネル行の左に引く、所属トラックの色帯の幅（ピクセル） */
constexpr float kChannelBarWidth = 2.0f;
/** @brief マウス位置の案内をカーソルから離す距離（ピクセル） */
constexpr float kHoverLabelOffset = 8.0f;
/** @brief 案内や凡例の文字まわりの余白（ピクセル） */
constexpr float kLabelPadding = 3.0f;
/** @brief カーブの凡例を画面の端から離す距離（ピクセル） */
constexpr float kCurveLegendMargin = 8.0f;
/** @brief カーブの凡例で、成分の色見本の一辺（ピクセル） */
constexpr float kCurveLegendSwatch = 10.0f;
/** @brief カーブの成分の名前。色は DrawCurveEditor の componentColors と同じ並び */
const char* const kComponentLabels[] = { "X", "Y", "Z", "W" };
/** @brief ドープシートで使える操作の案内 */
const char* const kDopeSheetHint =
	"トラック名の左の v / > : 値ごとの行を開閉\n"
	"行をダブルクリック: キーを打つ\n"
	"キーをクリック・ドラッグ: 選択と移動（Ctrl で追加選択）\n"
	"空いた所から引っ張る: 範囲選択\n"
	"右クリック: メニュー\n"
	"ホイール: 上下 / Shift+ホイール: 左右 / Ctrl+ホイール: 拡大縮小\n"
	"中ボタンドラッグ: 表示を動かす\n"
	"上の目盛りをドラッグ: 再生位置";
/** @brief カーブで使える操作の案内 */
const char* const kCurveHint =
	"ドープシートで選んだキー（無ければ選んだトラック）の値が、時間でどう変わるかの線。\n"
	"横が時間、縦が値、線の色が成分（右上の凡例）。\n"
	"点をドラッグ: 時刻と値を変える（Shift で値だけ）\n"
	"ベジェのキーを選ぶと、黄色い取っ手で曲がり方を変えられる\n"
	"ホイール: 縦の拡大 / Shift+ホイール: 横の拡大\n"
	"中ボタンドラッグ: 表示を動かす / F: 選んだ線に合わせる\n"
	"右クリック: メニュー";
/** @brief 設定欄の列の数（シーケンス・プレビュー・スナップ） */
constexpr int kSettingsColumnCount = 3;
/** @brief 設定欄で、シーケンスの列を他の列の何倍の幅にするか。項目が多いため */
constexpr float kSettingsSequenceColumnWeight = 2.0f;
/** @brief 設定欄で、入力欄が列の幅に占める割合。残りは見出しの文字に使う */
constexpr float kSettingsItemWidthRatio = 0.55f;
/** @brief ドープシート欄とカーブ欄の最低の高さ（ピクセル） */
constexpr float kPaneMinHeight = 80.0f;
/** @brief ドープシートとカーブの境目の太さ（ピクセル） */
constexpr float kPaneSplitterThickness = 6.0f;
/** @brief 案内の吹き出しを折り返す幅（文字の大きさの何倍か） */
constexpr float kHintWrapEm = 30.0f;
/** @brief 新規作成の小窓の ID */
const char* const kNewSequencePopupId = "新規作成###NewSequence";
/** @brief 名前を付けて保存の小窓の ID */
const char* const kSaveAsPopupId = "名前を付けて保存###SaveSequenceAs";
/** @brief シーケンスのファイルの拡張子 */
const char* const kSequenceFileExtension = ".json";

/**
 * @brief 欄の見出しと、マウスを乗せると操作の案内が出る (?) を描く
 * @param title 見出し
 * @param hint 案内の文章
 */
void DrawPaneHeader(const char* title, const char* hint)
{
	ImGui::TextUnformatted(title);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * kHintWrapEm);
		ImGui::TextUnformatted(hint);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

/**
 * @brief 打たれた名前をシーケンスのファイル名にする
 * @details 拡張子が無ければ .json を付ける。シーケンスのフォルダの外に作らないよう、区切り文字は受け付けない。
 * @param input 打たれた名前
 * @return ファイル名。使えない名前なら空
 */
std::string MakeSequenceFileName(const char* input)
{
	std::string name(input);
	if (name.empty() || name.find_first_of("/\\:") != std::string::npos)
	{
		return {};
	}
	if (std::filesystem::path(name).extension() != kSequenceFileExtension)
	{
		name += kSequenceFileExtension;
	}
	return name;
}

/**
 * @brief 画面上で目盛りが詰まりすぎない間隔を候補から選ぶ
 * @param steps 小さい順に並べた候補
 * @param pixelsPerUnit 1単位あたりのピクセル数
 * @param minSpacing 目盛りどうしに最低限あけるピクセル数
 * @return 条件を満たす一番小さい候補。どれも満たさなければ一番大きい候補
 */
template <size_t N>
float PickTickStep(const float (&steps)[N], float pixelsPerUnit, float minSpacing)
{
	for (float candidate : steps)
	{
		if (candidate * pixelsPerUnit >= minSpacing)
		{
			return candidate;
		}
	}
	return steps[N - 1];
}

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

	AddRequiredBindings();

	player_.SetSequence(&sequence_);

	// エディタでの再生中はイベントの発火を画面とログに出して確認できるようにする
	player_.SetEventCallback([this](const std::string& eventName)
	{
		statusMessage_ = "Event: " + eventName;
		Logger::Log("Sequencer Event: " + eventName + "\n");
	});

	DebugUIManager* debugUI = DebugUIManager::GetInstance();
	debugUI->RegisterWindow(this, "シーケンサ###Sequencer", [this]() { DrawTimelineWindow(); }, EditorDock::Bottom);
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
	pendingObjectBindings_.clear();
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

	// 名前で読んだ割り当ては、その GameObject が後から作られることもあるので探し続ける。
	// 見つかっていない物が無ければ何もしない（ふだんは毎フレームの負担にならない）
	if (!pendingObjectBindings_.empty())
	{
		ResolvePendingObjectBindings();
	}

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

void SequencerEditor::StorePreviewBindings()
{
	// 見つかっていない名前も書き戻す。シーンに居ないだけで、覚えていた割り当てを消さないため
	nlohmann::json objects = nlohmann::json::object();
	for (const auto& [role, name] : pendingObjectBindings_)
	{
		objects[role] = name;
	}
	for (const auto& [role, guid] : previewObjectBindings_)
	{
		const GameObject* object = GameObjectManager::HasInstance() ? GameObjectManager::GetInstance()->FindByGuid(guid) : nullptr;
		if (object)
		{
			objects[role] = object->GetName();
		}
	}

	nlohmann::json lights = nlohmann::json::object();
	for (const auto& [role, lightName] : previewLightBindings_)
	{
		lights[role] = lightName;
	}

	// 他のキーがあっても消さないよう、自分のキーだけを書き換える
	nlohmann::json& editorData = sequence_.GetEditorData();
	if (!editorData.is_object())
	{
		editorData = nlohmann::json::object();
	}
	editorData[kPreviewBindingsKey] = { { kPreviewObjectsKey, objects }, { kPreviewLightsKey, lights } };
}

void SequencerEditor::RestorePreviewBindings()
{
	const nlohmann::json& editorData = sequence_.GetEditorData();
	if (!editorData.is_object() || !editorData.contains(kPreviewBindingsKey) || !editorData[kPreviewBindingsKey].is_object())
	{
		return;
	}
	const nlohmann::json& bindings = editorData[kPreviewBindingsKey];

	// GameObject は名前しか分からないので、いったん「探し中」に入れて今いる物から探す
	if (bindings.contains(kPreviewObjectsKey) && bindings[kPreviewObjectsKey].is_object())
	{
		for (const auto& entry : bindings[kPreviewObjectsKey].items())
		{
			if (entry.value().is_string())
			{
				pendingObjectBindings_[entry.key()] = entry.value().get<std::string>();
			}
		}
	}
	if (bindings.contains(kPreviewLightsKey) && bindings[kPreviewLightsKey].is_object())
	{
		for (const auto& entry : bindings[kPreviewLightsKey].items())
		{
			if (entry.value().is_string())
			{
				previewLightBindings_[entry.key()] = entry.value().get<std::string>();
			}
		}
	}
	ResolvePendingObjectBindings();
}

void SequencerEditor::ResolvePendingObjectBindings()
{
	if (!GameObjectManager::HasInstance())
	{
		return;
	}
	const std::vector<GameObject*>& objects = GameObjectManager::GetInstance()->GetGameObjects();
	for (auto it = pendingObjectBindings_.begin(); it != pendingObjectBindings_.end();)
	{
		// 同じ名前が複数あるときは、一覧で先に出てくる物にする
		const auto found = std::find_if(objects.begin(), objects.end(),
			[&it](const GameObject* object) { return object && object->GetName() == it->second; });
		if (found != objects.end())
		{
			previewObjectBindings_[it->first] = (*found)->GetGuid();
			it = pendingObjectBindings_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void SequencerEditor::ClearPreviewBindings()
{
	// コンテキストに残った割り当ても外す。役の一覧から消すだけでは、前の物を動かし続けてしまう
	BindingContext& ctx = player_.GetBindingContext();
	for (const auto& [role, guid] : previewObjectBindings_)
	{
		(void)guid;
		ctx.BindGameObject(role, nullptr);
	}
	for (const auto& [role, lightName] : previewLightBindings_)
	{
		(void)lightName;
		ctx.BindLight(role, "");
	}
	previewObjectBindings_.clear();
	previewLightBindings_.clear();
	pendingObjectBindings_.clear();
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

	ImGui::SeparatorText("シーケンサ");
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
	if (ImGui::Button(isEditMode ? "モード: 編集" : "モード: 再生"))
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
	if (ImGui::Button(isPlaying ? "一時停止" : "再生"))
	{
		if (isPlaying) { player_.Pause(); }
		else { player_.Resume(); }
	}
	ImGui::SameLine();

	if (ImGui::Button("停止"))
	{
		player_.Stop();
	}
	ImGui::SameLine();

	if (ImGui::Button("末尾へ"))
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
	if (ImGui::Checkbox("ループ", &loop))
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
	if (ImGui::Button("トラックを追加"))
	{
		if (addTrackTypeIndex_ >= 0 && static_cast<size_t>(addTrackTypeIndex_) < typeCount)
		{
			AddTrack(typeNames[addTrackTypeIndex_]);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("キーを追加 (K)"))
	{
		AddKeyAtCurrentTime();
	}
	ImGui::SameLine();
	if (ImGui::Button("キーを削除 (Del)"))
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

	// 保存していない変更を捨ててよいかの確認。読み込みと新規作成で共用する
	if (discardPopupRequested_)
	{
		ImGui::OpenPopup(kDiscardChangesPopupId);
		discardPopupRequested_ = false;
	}
	if (ImGui::BeginPopupModal(kDiscardChangesPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const bool isNew = !pendingNewPath_.empty();
		ImGui::Text(isNew ? "保存していない変更があります。捨てて「%s」を新しく作りますか？" : "保存していない変更があります。捨てて「%s」を読み込みますか？",
			isNew ? pendingNewPath_.c_str() : pendingLoadPath_.c_str());
		if (ImGui::Button(isNew ? "捨てて作る" : "捨てて読み込む"))
		{
			if (isNew) { CreateNewSequence(pendingNewPath_); }
			else { LoadSequenceFile(pendingLoadPath_); }
			pendingLoadPath_.clear();
			pendingNewPath_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("やめる"))
		{
			pendingLoadPath_.clear();
			pendingNewPath_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

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

	const float step = PickTickStep(kTimeTickSteps, view_.pixelsPerSecond, kTimeTickMinSpacing);

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
			case TrackType::Particle: return IM_COL32(230, 150, 190, 255);
			default: return IM_COL32(110, 110, 120, 255);
			}
		}();
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
				// トラックの境目。どこから別のトラックかを分かりやすくする
				drawList->AddLine(ImVec2(canvasMin.x, rowTop), ImVec2(canvasMin.x + canvasSize.x, rowTop), IM_COL32(70, 70, 80, 255));
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

			// 値の行にもトラックの色を細く引いて、どのトラックの行かを分かるようにする
			drawList->AddRectFilled(ImVec2(canvasMin.x, rowTop), ImVec2(canvasMin.x + kChannelBarWidth, rowBottom), typeColor);

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

	// 行の背景でルーラーの目盛りが隠れるので、下のほうの行でも時刻を追えるよう薄く引き直す
	{
		const float gridStep = PickTickStep(kTimeTickSteps, view_.pixelsPerSecond, kTimeTickMinSpacing);
		const float gridTop = canvasMin.y + view_.rulerHeight;
		const float gridBottom = (std::min)(rowY, canvasMin.y + canvasSize.y);
		const float gridEnd = PixelToTime(canvasMin.x + canvasSize.x, left);
		for (int tick = static_cast<int>(std::floor(view_.scrollTime / gridStep)); static_cast<float>(tick) * gridStep <= gridEnd; ++tick)
		{
			const float x = TimeToPixel(static_cast<float>(tick) * gridStep, left);
			if (x < left) { continue; }
			drawList->AddLine(ImVec2(x, gridTop), ImVec2(x, gridBottom), IM_COL32(255, 255, 255, 14));
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

	// マウスの下の時刻と行の名前を出す。行が多いとルーラーも見出しも遠くて読みにくいため
	if (mouseInRows && !mouseInHeader)
	{
		const auto [hoverTrack, hoverRow] = findHitRow();
		ITrack* track = hoverTrack >= 0 ? sequence_.GetTrack(static_cast<size_t>(hoverTrack)) : nullptr;
		const float rowsOrigin = rowsTop - view_.verticalScroll;
		const float hoverTop = rowsOrigin + std::floor((mousePos.y - rowsOrigin) / view_.trackHeight) * view_.trackHeight;
		const float right = canvasMin.x + canvasSize.x;

		drawList->AddLine(ImVec2(mousePos.x, rowsTop), ImVec2(mousePos.x, canvasMin.y + canvasSize.y), IM_COL32(255, 255, 255, 60));
		if (track)
		{
			drawList->AddRect(ImVec2(canvasMin.x, hoverTop), ImVec2(right, hoverTop + view_.trackHeight), IM_COL32(255, 255, 255, 50));
		}

		char label[192];
		ICurveChannel* hoverChannel = track && hoverRow > 0 ? track->GetChannel(static_cast<size_t>(hoverRow - 1)) : nullptr;
		std::snprintf(label, sizeof(label), "%.2f s  %s%s%s",
			PixelToTime(mousePos.x, left),
			track ? track->GetName().c_str() : "",
			hoverChannel ? " / " : "",
			hoverChannel ? hoverChannel->GetName() : "");
		const ImVec2 textSize = ImGui::CalcTextSize(label);
		ImVec2 labelPos(mousePos.x + kHoverLabelOffset, (std::max)(hoverTop - textSize.y - kLabelPadding * 2.0f, rowsTop));
		if (labelPos.x + textSize.x + kLabelPadding * 2.0f > right)
		{
			labelPos.x = mousePos.x - kHoverLabelOffset - textSize.x - kLabelPadding * 2.0f;
		}
		drawList->AddRectFilled(labelPos, ImVec2(labelPos.x + textSize.x + kLabelPadding * 2.0f, labelPos.y + textSize.y + kLabelPadding * 2.0f), IM_COL32(15, 15, 18, 220));
		drawList->AddText(ImVec2(labelPos.x + kLabelPadding, labelPos.y + kLabelPadding), IM_COL32(230, 230, 235, 255), label);
	}

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

void SequencerEditor::FrameSelectedCurves()
{
	float minTime = FLT_MAX;
	float maxTime = -FLT_MAX;
	float minValue = FLT_MAX;
	float maxValue = -FLT_MAX;
	for (const SelectionItem& item : SelectionContext::GetInstance()->GetItems())
	{
		ICurveChannel* channel = GetSelectedChannel(sequence_, item);
		if (!channel || channel->GetComponentCount() == 0) { continue; }
		for (size_t key = 0; key < channel->GetKeyCount(); ++key)
		{
			minTime = (std::min)(minTime, channel->GetKeyTime(key));
			maxTime = (std::max)(maxTime, channel->GetKeyTime(key));
			for (size_t component = 0; component < channel->GetComponentCount(); ++component)
			{
				const float value = channel->GetKeyComponent(key, component);
				minValue = (std::min)(minValue, value);
				maxValue = (std::max)(maxValue, value);
			}
		}
	}
	if (minTime == FLT_MAX) { return; }
	constexpr float kMinimumTimeRange = 1.0f;
	constexpr float kMinimumValueRange = 1.0f;
	const ImVec2 available = ImGui::GetContentRegionAvail();
	const float timeRange = (std::max)(maxTime - minTime, kMinimumTimeRange);
	const float valueRange = (std::max)(maxValue - minValue, kMinimumValueRange);
	curveTimeStart_ = (std::max)(minTime - timeRange * 0.1f, 0.0f);
	curvePixelsPerSecond_ = (std::max)(available.x * 0.8f / timeRange, 5.0f);
	curveValueCenter_ = (minValue + maxValue) * 0.5f;
	curvePixelsPerValue_ = (std::max)(available.y * 0.7f / valueRange, 1.0f);
}

void SequencerEditor::DrawCurveEditor()
{
	const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	if (canvasSize.x < 50.0f || canvasSize.y < 50.0f) { return; }
	ImGui::InvisibleButton("CurveCanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const bool hovered = ImGui::IsItemHovered();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasMin, ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y), IM_COL32(25, 26, 30, 255));
	drawList->PushClipRect(canvasMin, ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y), true);

	const float centerY = canvasMin.y + canvasSize.y * 0.5f;
	auto timeToX = [&](float time) { return canvasMin.x + (time - curveTimeStart_) * curvePixelsPerSecond_; };
	auto valueToY = [&](float value) { return centerY - (value - curveValueCenter_) * curvePixelsPerValue_; };
	auto xToTime = [&](float x) { return curveTimeStart_ + (x - canvasMin.x) / curvePixelsPerSecond_; };
	auto yToValue = [&](float y) { return curveValueCenter_ + (centerY - y) / curvePixelsPerValue_; };

	// 目盛り。横が時間（下端に秒）、縦が値（左端に数字）
	const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
	const float labelHeight = ImGui::GetTextLineHeight();
	const float timeStep = PickTickStep(kTimeTickSteps, curvePixelsPerSecond_, kTimeTickMinSpacing);
	for (int tick = static_cast<int>(std::floor(curveTimeStart_ / timeStep)); static_cast<float>(tick) * timeStep <= xToTime(canvasMax.x); ++tick)
	{
		const float time = static_cast<float>(tick) * timeStep;
		const float x = timeToX(time);
		drawList->AddLine(ImVec2(x, canvasMin.y), ImVec2(x, canvasMax.y), IM_COL32(40, 40, 46, 255));
		char label[32];
		std::snprintf(label, sizeof(label), "%.2f s", time);
		drawList->AddText(ImVec2(x + kAxisLabelPadding, canvasMax.y - labelHeight - kAxisLabelPadding), IM_COL32(140, 140, 150, 255), label);
	}
	const float valueStep = PickTickStep(kValueTickSteps, curvePixelsPerValue_, kValueTickMinSpacing);
	for (int tick = static_cast<int>(std::floor(yToValue(canvasMax.y) / valueStep)); static_cast<float>(tick) * valueStep <= yToValue(canvasMin.y); ++tick)
	{
		const float value = static_cast<float>(tick) * valueStep;
		const float y = valueToY(value);
		drawList->AddLine(ImVec2(canvasMin.x, y), ImVec2(canvasMax.x, y), IM_COL32(40, 40, 46, 255));
		char label[32];
		std::snprintf(label, sizeof(label), "%g", value);
		drawList->AddText(ImVec2(canvasMin.x + kAxisLabelPadding, y - labelHeight), IM_COL32(140, 140, 150, 255), label);
	}
	drawList->AddLine(ImVec2(canvasMin.x, valueToY(0.0f)), ImVec2(canvasMin.x + canvasSize.x, valueToY(0.0f)), IM_COL32(70, 70, 76, 255));
	static const ImU32 componentColors[] = { IM_COL32(235, 85, 85, 255), IM_COL32(90, 220, 110, 255), IM_COL32(90, 140, 240, 255), IM_COL32(190, 190, 195, 255) };

	struct VisibleChannel { int track; int channel; ICurveChannel* curve; };
	std::vector<VisibleChannel> visible;
	for (const SelectionItem& item : SelectionContext::GetInstance()->GetItems())
	{
		if (item.trackIndex < 0 || item.channelIndex < 0) { continue; }
		ICurveChannel* channel = GetSelectedChannel(sequence_, item);
		if (!channel || channel->GetComponentCount() == 0) { continue; }
		const bool exists = std::any_of(visible.begin(), visible.end(), [&](const VisibleChannel& value)
		{
			return value.track == item.trackIndex && value.channel == item.channelIndex;
		});
		if (!exists) { visible.push_back({ item.trackIndex, item.channelIndex, channel }); }
	}
	if (visible.empty())
	{
		const int selectedTrack = GetSelectedTrackIndex();
		ITrack* track = selectedTrack >= 0 ? sequence_.GetTrack(static_cast<size_t>(selectedTrack)) : nullptr;
		for (size_t channelIndex = 0; track && channelIndex < track->GetChannelCount(); ++channelIndex)
		{
			ICurveChannel* channel = track->GetChannel(channelIndex);
			if (channel && channel->GetComponentCount() > 0) { visible.push_back({ selectedTrack, static_cast<int>(channelIndex), channel }); }
		}
	}

	constexpr int kSamplesPerSegment = 24;
	for (const VisibleChannel& entry : visible)
	{
		for (size_t component = 0; component < entry.curve->GetComponentCount(); ++component)
		{
			for (size_t key = 0; key + 1 < entry.curve->GetKeyCount(); ++key)
			{
				const float start = entry.curve->GetKeyTime(key);
				const float end = entry.curve->GetKeyTime(key + 1);
				ImVec2 previous(timeToX(start), valueToY(entry.curve->EvaluateComponent(start, component)));
				for (int sample = 1; sample <= kSamplesPerSegment; ++sample)
				{
					const float ratio = static_cast<float>(sample) / static_cast<float>(kSamplesPerSegment);
					const float time = start + (end - start) * ratio;
					const ImVec2 current(timeToX(time), valueToY(entry.curve->EvaluateComponent(time, component)));
					drawList->AddLine(previous, current, componentColors[component], 1.5f);
					previous = current;
				}
			}
			for (size_t key = 0; key < entry.curve->GetKeyCount(); ++key)
			{
				const ImVec2 point(timeToX(entry.curve->GetKeyTime(key)), valueToY(entry.curve->GetKeyComponent(key, component)));
				drawList->AddCircleFilled(point, 4.0f, componentColors[component]);
				if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					const ImVec2 mouse = ImGui::GetIO().MousePos;
					const float dx = mouse.x - point.x;
					const float dy = mouse.y - point.y;
					if (dx * dx + dy * dy <= kBezierHandleGrabRadius * kBezierHandleGrabRadius)
					{
						curveDragTrack_ = entry.track;
						curveDragChannel_ = entry.channel;
						curveDragKey_ = static_cast<int>(key);
						curveDragComponent_ = static_cast<int>(component);
						curveDragStartTime_ = entry.curve->GetKeyTime(key);
						curveDragStartValue_ = entry.curve->GetKeyComponent(key, component);
						curveDragMouseStart_ = mouse;
						curveDragCommand_ = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(entry.track), "Edit Curve Key");
					}
				}
			}
		}
	}

	// 再生位置。ドープシートと同じ赤線
	const float playheadX = timeToX(player_.GetTime());
	drawList->AddLine(ImVec2(playheadX, canvasMin.y), ImVec2(playheadX, canvasMax.y), IM_COL32(255, 90, 90, 255), 1.5f);

	if (visible.empty())
	{
		const char* const emptyText = "線で出せる値が選ばれていない。ドープシートで位置や色などのトラックかキーを選んでね（イベントなどは線にならない）";
		const ImVec2 textSize = ImGui::CalcTextSize(emptyText);
		drawList->AddText(ImVec2(canvasMin.x + (canvasSize.x - textSize.x) * 0.5f, canvasMin.y + (canvasSize.y - textSize.y) * 0.5f), IM_COL32(190, 190, 200, 255), emptyText);
	}

	// 凡例。どのトラックのどの値の線で、どの色が何の成分かを右上に出す
	float legendY = canvasMin.y + kCurveLegendMargin;
	for (const VisibleChannel& entry : visible)
	{
		ITrack* track = sequence_.GetTrack(static_cast<size_t>(entry.track));
		char name[160];
		std::snprintf(name, sizeof(name), "%s / %s", track ? track->GetName().c_str() : "", entry.curve->GetName());
		const size_t componentCount = (std::min)(entry.curve->GetComponentCount(), std::size(kComponentLabels));
		float width = ImGui::CalcTextSize(name).x;
		for (size_t component = 0; component < componentCount; ++component)
		{
			const char* componentLabel = componentCount == 1 ? "値" : kComponentLabels[component];
			width += kLabelPadding * 2.0f + kCurveLegendSwatch + kLabelPadding + ImGui::CalcTextSize(componentLabel).x;
		}
		float x = canvasMax.x - kCurveLegendMargin - width - kLabelPadding * 2.0f;
		drawList->AddRectFilled(ImVec2(x, legendY), ImVec2(canvasMax.x - kCurveLegendMargin, legendY + labelHeight + kLabelPadding * 2.0f), IM_COL32(15, 15, 18, 220));
		x += kLabelPadding;
		const float textY = legendY + kLabelPadding;
		drawList->AddText(ImVec2(x, textY), IM_COL32(220, 220, 228, 255), name);
		x += ImGui::CalcTextSize(name).x;
		for (size_t component = 0; component < componentCount; ++component)
		{
			const char* componentLabel = componentCount == 1 ? "値" : kComponentLabels[component];
			x += kLabelPadding * 2.0f;
			const float swatchY = textY + (labelHeight - kCurveLegendSwatch) * 0.5f;
			drawList->AddRectFilled(ImVec2(x, swatchY), ImVec2(x + kCurveLegendSwatch, swatchY + kCurveLegendSwatch), componentColors[component]);
			x += kCurveLegendSwatch + kLabelPadding;
			drawList->AddText(ImVec2(x, textY), IM_COL32(220, 220, 228, 255), componentLabel);
			x += ImGui::CalcTextSize(componentLabel).x;
		}
		legendY += labelHeight + kLabelPadding * 3.0f;
	}

	const SelectionItem primary = GetPrimarySelection();
	ICurveChannel* primaryChannel = GetSelectedChannel(sequence_, primary);
	if (primaryChannel && primaryChannel->HasInterpolation() && primary.keyIndex >= 0 &&
		static_cast<size_t>(primary.keyIndex + 1) < primaryChannel->GetKeyCount() &&
		primaryChannel->GetKeyInterp(static_cast<size_t>(primary.keyIndex)) == InterpolationMode::Bezier &&
		primaryChannel->GetComponentCount() > 0)
	{
		const size_t key = static_cast<size_t>(primary.keyIndex);
		const size_t component = static_cast<size_t>((std::max)(curveHandleComponent_, 0)) % primaryChannel->GetComponentCount();
		const float time0 = primaryChannel->GetKeyTime(key);
		const float time1 = primaryChannel->GetKeyTime(key + 1);
		const float value0 = primaryChannel->GetKeyComponent(key, component);
		const float value1 = primaryChannel->GetKeyComponent(key + 1, component);
		const float timeRange = time1 - time0;
		const float valueRange = value1 - value0;
		const BezierHandle& handle = primaryChannel->GetKeyBezier(key);
		const ImVec2 start(timeToX(time0), valueToY(value0));
		const ImVec2 end(timeToX(time1), valueToY(value1));
		const ImVec2 point1(timeToX(time0 + timeRange * handle.x1), valueToY(value0 + valueRange * handle.y1));
		const ImVec2 point2(timeToX(time0 + timeRange * handle.x2), valueToY(value0 + valueRange * handle.y2));
		drawList->AddLine(start, point1, IM_COL32(230, 180, 90, 220));
		drawList->AddLine(end, point2, IM_COL32(230, 180, 90, 220));
		drawList->AddCircleFilled(point1, kBezierHandleRadius, IM_COL32(255, 205, 100, 255));
		drawList->AddCircleFilled(point2, kBezierHandleRadius, IM_COL32(255, 205, 100, 255));
		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			const ImVec2 mouse = ImGui::GetIO().MousePos;
			auto nearPoint = [&](const ImVec2& point)
			{
				const float dx = mouse.x - point.x;
				const float dy = mouse.y - point.y;
				return dx * dx + dy * dy <= kBezierHandleGrabRadius * kBezierHandleGrabRadius;
			};
			if (nearPoint(point1) || nearPoint(point2))
			{
				curveHandleTrack_ = primary.trackIndex;
				curveHandleChannel_ = primary.channelIndex;
				curveHandleKey_ = primary.keyIndex;
				curveHandleComponent_ = static_cast<int>(component);
				curveHandlePoint_ = nearPoint(point1) ? 1 : 2;
				curveHandleCommand_ = std::make_unique<TrackEditCommand>(&sequence_, static_cast<size_t>(primary.trackIndex), "Edit Curve Handle");
			}
		}
	}

	if (curveHandleCommand_)
	{
		ITrack* track = sequence_.GetTrack(static_cast<size_t>(curveHandleTrack_));
		ICurveChannel* channel = track ? track->GetChannel(static_cast<size_t>(curveHandleChannel_)) : nullptr;
		if (channel && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const size_t key = static_cast<size_t>(curveHandleKey_);
			const float time0 = channel->GetKeyTime(key);
			const float time1 = channel->GetKeyTime(key + 1);
			const float value0 = channel->GetKeyComponent(key, static_cast<size_t>(curveHandleComponent_));
			const float value1 = channel->GetKeyComponent(key + 1, static_cast<size_t>(curveHandleComponent_));
			const float timeRange = (std::max)(time1 - time0, 0.0001f);
			const float valueRange = std::abs(value1 - value0) < 0.0001f ? 1.0f : value1 - value0;
			BezierHandle& handle = channel->GetKeyBezier(key);
			const float normalizedX = std::clamp((xToTime(ImGui::GetIO().MousePos.x) - time0) / timeRange, 0.0f, 1.0f);
			const float normalizedY = (yToValue(ImGui::GetIO().MousePos.y) - value0) / valueRange;
			if (curveHandlePoint_ == 1) { handle.x1 = (std::min)(normalizedX, handle.x2); handle.y1 = normalizedY; }
			else { handle.x2 = (std::max)(normalizedX, handle.x1); handle.y2 = normalizedY; }
		}
		else
		{
			curveHandleCommand_->CaptureAfter();
			if (curveHandleCommand_->HasChanged()) { CommandHistory::GetInstance()->Execute(std::move(curveHandleCommand_)); }
			else { curveHandleCommand_.reset(); }
			curveHandlePoint_ = 0;
		}
	}

	if (curveDragCommand_)
	{
		ICurveChannel* channel = sequence_.GetTrack(static_cast<size_t>(curveDragTrack_))->GetChannel(static_cast<size_t>(curveDragChannel_));
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const ImVec2 delta(ImGui::GetIO().MousePos.x - curveDragMouseStart_.x, ImGui::GetIO().MousePos.y - curveDragMouseStart_.y);
			if (!ImGui::GetIO().KeyShift)
			{
				curveDragKey_ = static_cast<int>(channel->MoveKey(static_cast<size_t>(curveDragKey_),
					(std::max)(curveDragStartTime_ + delta.x / curvePixelsPerSecond_, 0.0f)));
			}
			channel->SetKeyComponent(static_cast<size_t>(curveDragKey_), static_cast<size_t>(curveDragComponent_),
				curveDragStartValue_ - delta.y / curvePixelsPerValue_);
			player_.EvaluateCurrentTime();
		}
		else
		{
			curveDragCommand_->CaptureAfter();
			if (curveDragCommand_->HasChanged()) { CommandHistory::GetInstance()->Execute(std::move(curveDragCommand_)); }
			else { curveDragCommand_.reset(); }
			curveDragKey_ = -1;
		}
	}

	if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
	{
		const float factor = (std::max)(1.0f + ImGui::GetIO().MouseWheel * 0.1f, 0.1f);
		if (ImGui::GetIO().KeyShift) { curvePixelsPerSecond_ = std::clamp(curvePixelsPerSecond_ * factor, 5.0f, 2000.0f); }
		else { curvePixelsPerValue_ = std::clamp(curvePixelsPerValue_ * factor, 1.0f, 2000.0f); }
	}
	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) { curvePanning_ = true; }
	if (curvePanning_)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			curveTimeStart_ = (std::max)(curveTimeStart_ - ImGui::GetIO().MouseDelta.x / curvePixelsPerSecond_, 0.0f);
			curveValueCenter_ += ImGui::GetIO().MouseDelta.y / curvePixelsPerValue_;
		}
		else { curvePanning_ = false; }
	}
	if (hovered && ImGui::IsKeyPressed(ImGuiKey_F, false)) { FrameSelectedCurves(); }
	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		contextTime_ = (std::max)(xToTime(ImGui::GetIO().MousePos.x), 0.0f);
		ImGui::OpenPopup("TimelineContextMenu");
	}
	DrawTimelineContextMenu();
	drawList->PopClipRect();
}

void SequencerEditor::DrawTimelineWindow()
{
	// メニューバーは子窓に付ける。窓そのものは DebugUIManager が開くのでフラグを渡せない
	if (!ImGui::BeginChild("SequencerRoot", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_MenuBar))
	{
		ImGui::EndChild();
		return;
	}
	DrawMenuBar();
	DrawToolbar();
	ImGui::Separator();

	// ファイルと再生のすぐ下に設定を横に並べる。編集欄に横幅を全部使わせるため
	if (showSettingsPane_)
	{
		ImGui::BeginChild("SequencerSettings", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		DrawSettingsPane();
		ImGui::EndChild();
	}

	// 下: ドープシートとカーブを縦に並べる。片方だけを全面に出すこともできる
	ImGui::BeginChild("SequencerEditArea");
	editAreaHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

	// 表示の切り替え。今のものを押された色にしておく
	static const char* const kViewLabels[kEditAreaViewCount] = { "ドープシート", "カーブ", "両方" };
	for (int index = 0; index < kEditAreaViewCount; ++index)
	{
		if (index > 0) { ImGui::SameLine(); }
		const bool current = static_cast<int>(editAreaView_) == index;
		if (current) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
		if (ImGui::Button(kViewLabels[index])) { editAreaView_ = static_cast<EditAreaView>(index); }
		if (current) { ImGui::PopStyleColor(); }
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Tab で切り替え");

	const bool showDopeSheet = editAreaView_ != EditAreaView::Curve;
	const bool showCurve = editAreaView_ != EditAreaView::DopeSheet;
	const bool showBoth = showDopeSheet && showCurve;
	const ImGuiWindowFlags paneFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	const float spacing = ImGui::GetStyle().ItemSpacing.y;
	const float available = ImGui::GetContentRegionAvail().y;
	const float curveMaxHeight = (std::max)(available - kPaneMinHeight - kPaneSplitterThickness - spacing * 2.0f, kPaneMinHeight);
	curvePaneHeight_ = std::clamp(curvePaneHeight_, kPaneMinHeight, curveMaxHeight);
	const float dopeSheetHeight = showBoth ? available - curvePaneHeight_ - kPaneSplitterThickness - spacing * 2.0f : 0.0f;

	if (showDopeSheet)
	{
		ImGui::BeginChild("DopeSheetPane", ImVec2(0.0f, dopeSheetHeight), ImGuiChildFlags_None, paneFlags);
		DrawDopeSheet();
		ImGui::EndChild();
	}

	if (showBoth)
	{
		// 境目をドラッグしてドープシートとカーブの高さを分け直す
		ImGui::InvisibleButton("##PaneSplitter", ImVec2(-FLT_MIN, kPaneSplitterThickness));
		const bool splitterActive = ImGui::IsItemActive();
		if (splitterActive || ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		}
		if (splitterActive)
		{
			curvePaneHeight_ -= ImGui::GetIO().MouseDelta.y;
		}
		const ImVec2 splitterMin = ImGui::GetItemRectMin();
		const ImVec2 splitterMax = ImGui::GetItemRectMax();
		const float splitterY = (splitterMin.y + splitterMax.y) * 0.5f;
		ImGui::GetWindowDrawList()->AddLine(ImVec2(splitterMin.x, splitterY), ImVec2(splitterMax.x, splitterY),
			splitterActive ? IM_COL32(150, 170, 220, 255) : IM_COL32(70, 70, 80, 255), 2.0f);
	}

	if (showCurve)
	{
		ImGui::BeginChild("CurvePane", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, paneFlags);
		DrawPaneHeader("カーブ", kCurveHint);
		DrawCurveEditor();
		ImGui::EndChild();
	}
	ImGui::EndChild();
	ImGui::EndChild();
}

void SequencerEditor::DrawSettingsPane()
{
	// シーケンス・プレビュー・スナップを横に3列で並べる。シーケンスは項目が多いので広めに取る
	if (!ImGui::BeginTable("SequencerSettingsColumns", kSettingsColumnCount, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}
	ImGui::TableSetupColumn("シーケンス", ImGuiTableColumnFlags_WidthStretch, kSettingsSequenceColumnWeight);
	ImGui::TableSetupColumn("プレビュー", ImGuiTableColumnFlags_WidthStretch, 1.0f);
	ImGui::TableSetupColumn("スナップ", ImGuiTableColumnFlags_WidthStretch, 1.0f);

	ImGui::TableNextColumn();
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * kSettingsItemWidthRatio);
	ImGui::SeparatorText("シーケンス");
	{
		SequenceMeta& meta = sequence_.GetMeta();

		char nameBuffer[128];
		std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", meta.name.c_str());
		if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer)))
		{
			meta.name = nameBuffer;
		}

		const char* audioPreview = meta.audioClip.empty() ? "(なし)" : meta.audioClip.c_str();
		if (ImGui::BeginCombo("オーディオクリップ", audioPreview))
		{
			if (ImGui::Selectable("(なし)", meta.audioClip.empty()))
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
		if (ImGui::Button("テンポをタップ"))
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
		const bool offsetChanged = ImGui::DragFloat("オフセット", &offset, 0.001f, -10.0f, 10.0f, "%.3f s");
		if (ImGui::IsItemActivated()) { ++metaEditId_; }
		if (offsetChanged)
		{
			SequenceMeta after = meta;
			after.offset = offset;
			ExecuteMetaEdit(sequence_, after, "Change Offset", metaEditId_);
		}
		ImGui::SameLine();
		if (ImGui::Button("再生ヘッドに合わせる"))
		{
			++metaEditId_;
			SequenceMeta after = meta;
			after.offset = player_.GetTime();
			ExecuteMetaEdit(sequence_, after, "Set Offset to Playhead", metaEditId_);
		}
		ImGui::DragFloat("長さ", &meta.duration, 0.1f, 0.0f, 3600.0f, "%.2f s");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("0 にするとトラックの最終キーから自動で決まります");
		}
	}

	ImGui::PopItemWidth();

	ImGui::TableNextColumn();
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * kSettingsItemWidthRatio);
	ImGui::SeparatorText("プレビュー");
	if (ImGui::Checkbox("シーケンスカメラで見る", &previewThroughSequenceCamera_))
	{
		ApplyActiveCamera();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("オフの間は編集用カメラから眺め、シーケンスカメラをギズモで操作できます");
	}
	ImGui::Combo("ギズモ", &gizmoOperation_, "移動\0回転\0スケール\0");
	ImGui::Checkbox("ワールド座標で動かす", &gizmoWorldSpace_);
	ImGui::DragFloat("カメラの移動速度", &editorCameraSpeed_, 0.1f, 0.1f, 200.0f, "%.1f");

	ImGui::PopItemWidth();

	ImGui::TableNextColumn();
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * kSettingsItemWidthRatio);
	ImGui::SeparatorText("スナップ");
	ImGui::Checkbox("スナップする", &snapEnabled_);
	ImGui::DragFloat("間隔", &snapInterval_, 0.01f, 0.01f, 5.0f, "%.2f s");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("BPM を入れているときは拍に吸い付きます");
	}

	ImGui::PopItemWidth();
	ImGui::EndTable();
}

void SequencerEditor::DrawDopeSheet()
{
	DrawPaneHeader("ドープシート", kDopeSheetHint);
	ImGui::SameLine();

	// ズーム
	ImGui::SetNextItemWidth(200.0f);
	ImGui::DragFloat("ズーム", &view_.pixelsPerSecond, 1.0f, 5.0f, 2000.0f, "%.0f px/s");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	ImGui::DragFloat("スクロール", &view_.scrollTime, 0.05f, 0.0f, 3600.0f, "%.2f s");
	ImGui::SameLine();
	if (ImGui::Button("全体を表示 (F)"))
	{
		FrameAllKeys();
	}


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

	ImGui::SeparatorText("補間（このキーから次のキーまで）");

	int interpIndex = static_cast<int>(interp);
	if (ImGui::Combo("モード", &interpIndex, "一定\0直線\0ベジェ\0"))
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

	ImGui::SeparatorText("トラック");
	ImGui::Text("種類: %s", track->GetTypeName());

	// 名前
	char nameBuffer[128];
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", track->GetName().c_str());
	if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer)))
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

	ImGui::SeparatorText("割り当て");

	// 役の名前
	char roleBuffer[64];
	std::snprintf(roleBuffer, sizeof(roleBuffer), "%s", track->GetBindingRole().c_str());
	if (ImGui::InputText("役", roleBuffer, sizeof(roleBuffer)))
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

	ImGui::TextDisabled("プレビュー用の割り当て（エディタのみ。シーケンスと一緒に名前で保存）");

	switch (bindingType)
	{
	case BindingType::Camera:
		ImGui::TextDisabled("カメラの役 \"%s\" はシーケンスカメラに割り当て済みです", kCameraRole);
		break;

	case BindingType::GameObject:
		DrawPreviewObjectCombo(role, "プレビュー対象");
		break;

	case BindingType::Light:
	{
		auto* lightTrack = dynamic_cast<LightTrack*>(track);
		const auto it = previewLightBindings_.find(role);
		const std::string current = (it != previewLightBindings_.end()) ? it->second : std::string();

		if (ImGui::BeginCombo("プレビューライト", current.empty() ? "(未割り当て)" : current.c_str()))
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

	// ファイルで覚えていた名前の物が今のシーンに居ないときは、それが分かるように出す
	const auto pending = pendingObjectBindings_.find(role);
	std::string preview = "(未割り当て)";
	if (current) { preview = current->GetName(); }
	else if (pending != pendingObjectBindings_.end()) { preview = "(見つからない: " + pending->second + ")"; }

	// 割り当ての変更は Undo で戻せるようにし、保存していない印も付ける
	const auto bind = [this, &role](GameObject* object)
	{
		pendingObjectBindings_.erase(role);
		CommandHistory::GetInstance()->Execute(std::make_unique<PreviewObjectBindingCommand>(
			&previewObjectBindings_, role, object ? object->GetGuid() : Guid{}, object == nullptr));
		player_.GetBindingContext().BindGameObject(role, object);
		ApplyPreviewBindings();
		player_.EvaluateCurrentTime();
	};

	if (ImGui::BeginCombo(label, preview.c_str()))
	{
		if (ImGui::Selectable("(未割り当て)", current == nullptr))
		{
			bind(nullptr);
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
					bind(object);
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

		ImGui::SeparatorText("キー");
		ImGui::Text("チャンネル: %s  /  キー: %zu", channel->GetName(), keyIndex);

		float keyTime = channel->GetKeyTime(keyIndex);
		if (ImGui::DragFloat("時刻", &keyTime, 0.01f, 0.0f, 3600.0f, "%.3f s"))
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

void SequencerEditor::RefreshSequenceFileList()
{
	sequenceFiles_.clear();
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(Sequence::GetSequenceDirectory(), error))
	{
		if (entry.is_regular_file(error) && entry.path().extension() == ".json")
		{
			sequenceFiles_.push_back(entry.path().filename().string());
		}
	}
	std::sort(sequenceFiles_.begin(), sequenceFiles_.end());
}

void SequencerEditor::RequestLoadSequenceFile(const std::string& path)
{
	if (CommandHistory::GetInstance()->IsDirty())
	{
		pendingLoadPath_ = path;
		discardPopupRequested_ = true;
		return;
	}
	LoadSequenceFile(path);
}

void SequencerEditor::LoadSequenceFile(const std::string& path)
{
	// 読み込みでシーケンスの中身が入れ替わるため、選択と履歴を捨てる。
	// 残すと、消えたトラックを指したままの選択やUndoが残ってしまう。
	player_.Stop();
	SelectionContext::GetInstance()->ClearSelection();
	CommandHistory::GetInstance()->Clear();

	std::string error;
	if (sequence_.LoadFromFile(path, &error))
	{
		// 読めたファイルを保存先にもする。別のファイルに上書きしてしまわないように
		filePath_ = path;
		statusMessage_ = "読み込みました: " + path;
		// 前のシーケンスの割り当てを残すと、同じ役名の別の物を動かしてしまう
		ClearPreviewBindings();
		RestorePreviewBindings();
		ApplyPreviewBindings();
		player_.EvaluateCurrentTime();
	}
	else
	{
		statusMessage_ = "読み込みに失敗しました: " + error;
	}
}

void SequencerEditor::SaveSequenceFile(const std::string& path)
{
	StorePreviewBindings();
	if (sequence_.SaveToFile(path))
	{
		filePath_ = path;
		statusMessage_ = "保存しました: " + path;
		CommandHistory::GetInstance()->MarkSaved();
	}
	else
	{
		statusMessage_ = "保存に失敗しました: " + path;
	}
}

void SequencerEditor::RequestNewSequence(const std::string& path)
{
	if (CommandHistory::GetInstance()->IsDirty())
	{
		pendingNewPath_ = path;
		discardPopupRequested_ = true;
		return;
	}
	CreateNewSequence(path);
}

void SequencerEditor::CreateNewSequence(const std::string& path)
{
	// 読み込みと同じく中身が入れ替わるので、選択と履歴を捨てる
	player_.Stop();
	SelectionContext::GetInstance()->ClearSelection();
	CommandHistory::GetInstance()->Clear();
	collapsedTracks_.clear();
	ClearPreviewBindings();

	sequence_.Clear();
	AddRequiredBindings();
	sequence_.GetMeta().name = std::filesystem::path(path).stem().string();

	// すぐ保存して、一覧に出るようにしておく
	SaveSequenceFile(path);
	if (filePath_ == path)
	{
		statusMessage_ = "新しく作りました: " + path;
	}
	player_.EvaluateCurrentTime();
}

void SequencerEditor::AddRequiredBindings()
{
	// シーケンスは必ず MainCam の役を要求する
	BindingDefinition binding;
	binding.role = kCameraRole;
	binding.type = BindingType::Camera;
	binding.description = "シーケンスが動かすカメラ";
	sequence_.AddBinding(binding);
}

void SequencerEditor::DrawMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("ファイル"))
		{
			if (ImGui::MenuItem("新規作成..."))
			{
				fileNameInput_.fill('\0');
				newPopupRequested_ = true;
			}
			if (ImGui::BeginMenu("開く"))
			{
				// 開いたときだけフォルダを読み直す。開いている間に毎フレーム読むと重い
				if (ImGui::IsWindowAppearing())
				{
					RefreshSequenceFileList();
				}
				if (sequenceFiles_.empty())
				{
					ImGui::TextDisabled("まだありません");
				}
				for (const std::string& file : sequenceFiles_)
				{
					if (ImGui::MenuItem(file.c_str(), nullptr, file == filePath_))
					{
						RequestLoadSequenceFile(file);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("保存", "Ctrl+S"))
			{
				SaveSequenceFile(filePath_);
			}
			if (ImGui::MenuItem("名前を付けて保存..."))
			{
				std::snprintf(fileNameInput_.data(), fileNameInput_.size(), "%s", filePath_.c_str());
				saveAsPopupRequested_ = true;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("表示"))
		{
			ImGui::MenuItem("設定欄", nullptr, &showSettingsPane_);
			ImGui::Separator();
			if (ImGui::MenuItem("ドープシート", "Tab", editAreaView_ == EditAreaView::DopeSheet)) { editAreaView_ = EditAreaView::DopeSheet; }
			if (ImGui::MenuItem("カーブ", "Tab", editAreaView_ == EditAreaView::Curve)) { editAreaView_ = EditAreaView::Curve; }
			if (ImGui::MenuItem("両方", "Tab", editAreaView_ == EditAreaView::Both)) { editAreaView_ = EditAreaView::Both; }
			ImGui::Separator();
			if (ImGui::MenuItem("全体を表示", "F"))
			{
				FrameAllKeys();
			}
			ImGui::EndMenu();
		}

		// 今どのファイルを触っているかを右端に出す。* は保存していない変更あり
		char fileLabel[160];
		std::snprintf(fileLabel, sizeof(fileLabel), "%s%s", filePath_.c_str(), CommandHistory::GetInstance()->IsDirty() ? " *" : "");
		const float labelX = ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(fileLabel).x;
		if (labelX > ImGui::GetCursorPosX())
		{
			ImGui::SetCursorPosX(labelX);
		}
		ImGui::TextUnformatted(fileLabel);
		ImGui::EndMenuBar();
	}

	// 小窓はメニューの外で開く。メニューの中で開くと ID がずれて開かない
	if (newPopupRequested_)
	{
		ImGui::OpenPopup(kNewSequencePopupId);
		newPopupRequested_ = false;
	}
	if (saveAsPopupRequested_)
	{
		ImGui::OpenPopup(kSaveAsPopupId);
		saveAsPopupRequested_ = false;
	}

	// 新規作成と名前を付けて保存は、名前を打つところが同じなのでまとめて描く
	const auto drawFileNamePopup = [this](const char* popupId, bool isNew)
	{
		if (!ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			return;
		}
		ImGui::TextUnformatted(isNew ? "新しいシーケンスのファイル名" : "保存するファイル名");
		ImGui::TextDisabled("置き場所: Resources/json/sequence（.json は省略できる）");
		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}
		const bool entered = ImGui::InputText("##SequenceFileName", fileNameInput_.data(), fileNameInput_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
		const std::string path = MakeSequenceFileName(fileNameInput_.data());
		std::error_code error;
		const bool exists = !path.empty() && std::filesystem::exists(Sequence::GetSequenceDirectory() / path, error);
		if (fileNameInput_[0] != '\0' && path.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "/ \\ : は使えない");
		}
		else if (exists)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), isNew ? "同じ名前のファイルがある。別の名前にしてね" : "同じ名前のファイルがある。保存すると上書きする");
		}

		// 新規作成で既存を潰すと演出データが消えるので、上書きは名前を付けて保存のときだけ許す
		const bool canConfirm = !path.empty() && (!isNew || !exists);
		ImGui::BeginDisabled(!canConfirm);
		const char* confirmLabel = isNew ? "作る" : (exists ? "上書きして保存" : "保存");
		if (ImGui::Button(confirmLabel) || (entered && canConfirm))
		{
			if (isNew) { RequestNewSequence(path); }
			else { SaveSequenceFile(path); }
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("やめる"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	};
	drawFileNamePopup(kNewSequencePopupId, true);
	drawFileNamePopup(kSaveAsPopupId, false);
}

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

	// 編集欄の上でだけ効かせる。他の窓で Tab を使う操作とぶつけないため
	if (editAreaHovered_ && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Tab, false))
	{
		editAreaView_ = static_cast<EditAreaView>((static_cast<int>(editAreaView_) + 1) % kEditAreaViewCount);
	}

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
		SaveSequenceFile(filePath_);
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
