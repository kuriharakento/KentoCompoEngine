#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "core/Guid.h"
#include "sequencer/core/Sequence.h"
#include "sequencer/core/SequencePlayer.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
class BeamRenderer;
class Camera;
class CameraManager;
class FogRenderer;
class LightManager;
class PostProcessManager;

#ifdef USE_IMGUI

/**
 * @brief タイムラインの表示設定
 */
struct TimelineViewState
{
	//! 表示開始時刻（秒）
	float scrollTime = 0.0f;
	//! 1秒あたりのピクセル数
	float pixelsPerSecond = 120.0f;
	//! トラック1本の高さ（ピクセル）
	float trackHeight = 26.0f;
	//! 左側のトラック名欄の幅（ピクセル）
	float headerWidth = 180.0f;
	//! 時間ルーラーの高さ（ピクセル）
	float rulerHeight = 24.0f;
};

/**
 * @brief 演出シーケンサのエディタUI
 *
 * @details キーの編集はトラックの種類を問わず ICurveChannel だけを通して行う。
 *          トラックを1種類足しても、このクラスを書き換えずに済むようにするため
 *          （SEQUENCER_PLAN Phase 6「カーブ編集の共通化」）。
 *
 *          編集はすべて CommandHistory 経由、選択はすべて SelectionContext 経由で行う。
 *          このクラス内に独自の選択状態や独自の履歴を持たせないこと。
 */
class SequencerEditor
{
public:
	static SequencerEditor* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 初期化してデバッグUIを登録する
	 * @param cameraManager 編集用カメラの追加先。nullptrならカメラ操作機能は無効になる
	 * @param lightManager ライトトラックが駆動するライト管理
	 * @param postProcessManager ポストプロセストラックが駆動するポストプロセス
	 */
	void Initialize(CameraManager* cameraManager, LightManager* lightManager, PostProcessManager* postProcessManager);

	/**
	 * @brief 終了処理
	 */
	void Finalize();

	/**
	 * @brief 毎フレームの更新
	 * @details ショートカットの処理と、再生位置の更新を行う。
	 *          ImGuiのフレーム内から呼ぶこと（ショートカット判定にImGuiの入力を使うため）。
	 */
	void Update();

	Sequence& GetSequence() { return sequence_; }
	SequencePlayer& GetPlayer() { return player_; }

	/**
	 * @brief シーケンスが駆動するカメラを設定する
	 * @details 役 "MainCam" に割り当てる。
	 * @param camera 対象のカメラ
	 */
	void SetSequenceCamera(Camera* camera);

	/**
	 * @brief トラックが駆動する大気（フォグとビーム）を設定する
	 * @details 大気はシーケンサより後に作られるため、初期化とは別に渡す。
	 */
	void SetAtmosphere(FogRenderer* fogRenderer, BeamRenderer* beamRenderer);

public:
	~SequencerEditor() = default;

private:
	static std::unique_ptr<SequencerEditor> instance_;
	friend std::unique_ptr<SequencerEditor> std::make_unique<SequencerEditor>();

	SequencerEditor() = default;
	SequencerEditor(const SequencerEditor&) = delete;
	SequencerEditor& operator=(const SequencerEditor&) = delete;

	// --- UI描画 ---

	/** @brief タイムラインウィンドウ（Projectエリア） */
	void DrawTimelineWindow();
	/** @brief インスペクタ（Inspectorエリア）。選択中のトラック・キーを編集する */
	void DrawInspectorWindow();
	/** @brief シーンへのオーバーレイ（Sceneエリア）。ギズモを描く */
	void DrawSceneOverlay();

	/** @brief 再生・保存などのツールバー */
	void DrawToolbar();
	/** @brief 時間ルーラーとビートグリッド */
	void DrawRuler(const ImVec2& canvasMin, float canvasWidth);
	/** @brief トラック行とキーを描く */
	void DrawTracks(const ImVec2& canvasMin, const ImVec2& canvasSize);
	/** @brief 再生ヘッドを描き、ドラッグによるスクラブを処理する */
	void DrawPlayhead(const ImVec2& canvasMin, const ImVec2& canvasSize);
	/** @brief 選択中のキーのベジェハンドルを編集するUI */
	void DrawBezierEditor();
	/** @brief トラック自体の設定（名前・役・プレビュー用の割り当て） */
	void DrawTrackInspector(size_t trackIndex);

	// --- 操作 ---

	/** @brief キーボードショートカットを処理する */
	void HandleShortcuts();
	/** @brief 対象の現在の状態を、現在時刻にキーとして打つ */
	void AddKeyAtCurrentTime();
	/** @brief 選択中のキーを削除する */
	void DeleteSelectedKey();
	/** @brief 指定種別のトラックを追加する */
	void AddTrack(const std::string& typeName);
	/** @brief 編集用カメラを自由移動させる */
	void UpdateEditorCameraFly();
	/** @brief 現在のプレビュー対象に応じてアクティブカメラを切り替える */
	void ApplyActiveCamera();
	/**
	 * @brief プレビュー用の割り当てをバインディングコンテキストへ反映する
	 * @details GameObject は GUID で覚えておき、毎フレーム引き直す。
	 *          ポインタで覚えると、対象が破棄されたときにダングリングする。
	 */
	void ApplyPreviewBindings();

	/** @brief 選択から、操作対象のトラック番号を求める。無ければ -1 */
	int GetSelectedTrackIndex() const;

	// --- 座標変換 ---

	/** @brief 時刻をキャンバス上のX座標に変換する */
	float TimeToPixel(float time, float canvasLeft) const;
	/** @brief キャンバス上のX座標を時刻に変換する */
	float PixelToTime(float pixelX, float canvasLeft) const;
	/** @brief 設定に応じて時刻をグリッドにスナップする */
	float SnapTime(float time) const;

	/** @brief 現在シーケンスを駆動しているカメラを返す */
	Camera* GetSequenceCamera() const;

	// 編集中のシーケンス
	Sequence sequence_;
	// 再生を司るプレイヤー
	SequencePlayer player_;
	// タイムラインの表示状態
	TimelineViewState view_;

	// カメラの追加先
	CameraManager* cameraManager_ = nullptr;
	// ライトトラックの駆動先
	LightManager* lightManager_ = nullptr;
	// ポストプロセストラックの駆動先
	PostProcessManager* postProcessManager_ = nullptr;
	// シーケンスが駆動するカメラの名前
	std::string sequenceCameraName_;
	// 編集用の自由移動カメラの名前
	std::string editorCameraName_;

	// エディタでのプレビュー用の割り当て（役 → GameObject の GUID）
	std::unordered_map<std::string, Guid> previewObjectBindings_;
	// エディタでのプレビュー用の割り当て（役 → ライト名）
	std::unordered_map<std::string, std::string> previewLightBindings_;

	// 「トラックを追加」で選んでいる種別
	int addTrackTypeIndex_ = 0;

	// シーケンスカメラ視点でプレビューするか
	bool previewThroughSequenceCamera_ = false;
	// ギズモの操作モード（0:移動 1:回転）
	int gizmoOperation_ = 0;
	// ギズモをワールド座標で操作するか
	bool gizmoWorldSpace_ = true;

	// グリッドスナップを有効にするか
	bool snapEnabled_ = true;
	// スナップの間隔（秒）。BPMが設定されている場合は拍に従う
	float snapInterval_ = 0.1f;

	// 保存先のファイル名
	std::string filePath_ = "sequence.json";
	// 最後の保存・読み込みの結果メッセージ
	std::string statusMessage_;

	// タイムライン上でキーをドラッグ中かどうか
	bool draggingKey_ = false;
	// 再生ヘッドをドラッグ中かどうか
	bool draggingPlayhead_ = false;
	// 編集用カメラの移動速度
	float editorCameraSpeed_ = 8.0f;
	// 初期化済みかどうか
	bool initialized_ = false;
};

#else

/**
 * @brief 非ImGui環境向けの空実装
 * @details リリースビルドでもシーケンス自体は再生できるよう、
 *          エディタUIだけを取り除く。
 */
class SequencerEditor
{
public:
	static SequencerEditor* GetInstance();
	static bool HasInstance();

	void Initialize(CameraManager* cameraManager, LightManager* lightManager, PostProcessManager* postProcessManager)
	{
		(void)cameraManager;
		(void)lightManager;
		(void)postProcessManager;
	}
	void Finalize() {}
	void Update() {}

	Sequence& GetSequence() { return sequence_; }
	SequencePlayer& GetPlayer() { return player_; }
	void SetSequenceCamera(Camera* camera) { (void)camera; }
	void SetAtmosphere(FogRenderer* fogRenderer, BeamRenderer* beamRenderer)
	{
		player_.GetBindingContext().SetFogRenderer(fogRenderer);
		player_.GetBindingContext().SetBeamRenderer(beamRenderer);
	}

public:
	~SequencerEditor() = default;

private:
	static std::unique_ptr<SequencerEditor> instance_;
	friend std::unique_ptr<SequencerEditor> std::make_unique<SequencerEditor>();

	SequencerEditor() = default;
	SequencerEditor(const SequencerEditor&) = delete;
	SequencerEditor& operator=(const SequencerEditor&) = delete;

	Sequence sequence_;
	SequencePlayer player_;
};

#endif // USE_IMGUI
} // namespace KCE
