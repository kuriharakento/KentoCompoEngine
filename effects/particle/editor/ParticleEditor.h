#pragma once
/**
 * @file ParticleEditor.h
 * @brief パーティクルエディタ
 * 
 * ImGuiベースのリアルタイムパーティクル編集UI。
 * エフェクトの作成、編集、保存、読み込みが可能。
 */
#include <string>
#include <memory>
#include <functional>
#include "math/Vector3.h"
#include "math/Vector4.h"

namespace KCE
{
class ParticleEffect;
class ParticleEmitter;
class DirectXCommon;
class SrvManager;
class CameraManager;
class PostProcessManager;

/**
 * @brief パーティクルエディタ
 * 
 * ImGuiベースのリアルタイムパーティクル編集UI。
 * エフェクトの作成、編集、保存、読み込みが可能。
 */
class ParticleEditor
{
public:
	ParticleEditor();
	~ParticleEditor();

	/**
	 * @brief 初期化
	 * @param dxCommon DirectXCommonポインタ
	 * @param srvManager SrvManagerポインタ
	 * @param postProcessManager PostProcessManagerポインタ（Emissive有効化時に選択的ブルームを自動有効化するために使用。nullptr可）
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, PostProcessManager* postProcessManager = nullptr);

	/**
	 * @brief 更新（ImGui描画）
	 * @param camera カメラマネージャー
	 */
	void Update(CameraManager* camera);

	/**
	 * @brief デバッグ表示描画（エミッター位置、パーティクル位置をライン描画）
	 */
	void DrawDebug();

	void DrawImGui();

	/**
	 * @brief エディタの表示状態を設定
	 * @param visible 表示する場合true
	 */
	void SetVisible(bool visible) { isVisible_ = visible; }
	
	/**
	 * @brief エディタの表示状態を取得
	 * @return 表示中の場合true
	 */
	bool IsVisible() const { return isVisible_; }
	
	/**
	 * @brief エディタの表示/非表示を切り替え
	 */
	void ToggleVisible() { isVisible_ = !isVisible_; }

	/**
	 * @brief 天球（Skydome）の色を取得する
	 */
	Vector4 GetSkydomeColor() const { return skydomeColor_; }

	/**
	 * @brief 新規エフェクトを作成
	 */
	void NewEffect();

	/**
	 * @brief 現在のエフェクトを閉じる（ポインタを安全に切り離す）
	 */
	void CloseCurrentEffect();

	/**
	 * @brief エフェクトを読み込み
	 * @param path JSONファイルパス
	 */
	bool LoadEffect(const std::string& path);

	/**
	 * @brief エフェクトを保存
	 * @param path 保存先JSONファイルパス
	 */
	bool SaveEffect(const std::string& path);

	/**
	 * @brief 編集中のエフェクトを取得
	 * @return 現在のエフェクト（nullptrの可能性あり）
	 */
	ParticleEffect* GetCurrentEffect() const { return currentEffect_; }

	/** Inspectorで表示するEmitterを安全に選択する。範囲外なら選択解除。 */
	bool SelectEmitter(size_t index);

private:
	/** インスペクタに表示する対象 */
	enum class InspectorTarget
	{
		Effect,
		Emitter,
		Module,
		Renderer
	};

	/**
	 * @brief ツールバー（ファイル操作・再生制御・表示設定）を描画
	 */
	void DrawToolbar();

	/**
	 * @brief アウトライナー（エミッター一覧＋モジュールスタック）を描画
	 */
	void DrawOutliner();

	/**
	 * @brief アウトライナー内のモジュールスタックを描画
	 * @param emitter 対象エミッター
	 * @param emitterIndex 対象エミッターのindex（選択時に使用）
	 */
	void DrawModuleStack(ParticleEmitter* emitter, size_t emitterIndex);

	/**
	 * @brief 選択対象に応じたインスペクタを描画
	 */
	void DrawInspector();

	/**
	 * @brief エフェクト設定パネルを描画
	 */
	void DrawEffectPanel();

	/**
	 * @brief エミッター設定パネルを描画
	 */
	void DrawEmitterPanel();

	/**
	 * @brief エミッターパラメータ・Dynamic Inputを描画
	 * @param emitter 対象エミッター
	 */
	void DrawEmitterParameterPanel(ParticleEmitter* emitter);

	/**
	 * @brief レンダラー設定パネルを描画
	 */
	void DrawRendererPanel();

	/**
	 * @brief モジュールのプロパティUIを描画
	 * @param module モジュールポインタ
	 */
	void DrawModuleProperties(class IModule* module);

	/**
	 * @brief エミッター追加ダイアログを表示
	 */
	void AddEmitterDialog();

	/**
	 * @brief モジュール追加ダイアログを表示
	 * @param emitter 対象エミッター
	 */
	void AddModuleDialog(ParticleEmitter* emitter);

	/**
	 * @brief 名前を付けて保存ダイアログを表示
	 */
	void SaveAsDialog();

	/**
	 * @brief 未保存の変更を破棄するか確認するダイアログを表示
	 */
	void DiscardConfirmDialog();

	/**
	 * @brief 未保存の変更があれば確認してからactionを実行する
	 */
	void RunWithDiscardCheck(std::function<void()> action);

	/** エミッターを複製して末尾に追加する */
	void DuplicateEmitter(size_t index);

	/** 指定エミッター以外をミュートする（既にソロなら全員のミュートを解除） */
	void SoloEmitter(size_t index);

	/** エミッターのミュート状態を設定する（ミュート中は生成せず既存パーティクルも消す） */
	void SetEmitterMuted(ParticleEmitter* emitter, bool muted);

	/** 選択中のモジュールを取得（エミッター内に存在しなければnullptr） */
	class IModule* GetSelectedModule(ParticleEmitter* emitter) const;

	/** 選択中のエミッターを取得 */
	ParticleEmitter* GetSelectedEmitter() const;

	void MarkDirty() { isDirty_ = true; }

private:
	ParticleEffect* currentEffect_ = nullptr;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	PostProcessManager* postProcessManager_ = nullptr;

	bool isVisible_ = false;
	bool showDebug_ = true;  // デバッグ表示ON/OFF
	bool showParticleMarkers_ = false; // パーティクルクロスマーカー表示ON/OFF
	bool showAddEmitterDialog_ = false;
	bool showAddModuleDialog_ = false;
	bool showSaveAsDialog_ = false;
	int selectedEmitterIndex_ = -1;
	// モジュールはエミッター内で優先度ソートされindexが変わるため、ポインタで選択を保持する
	class IModule* selectedModule_ = nullptr;
	InspectorTarget inspectorTarget_ = InspectorTarget::Effect;

	// 未保存の変更管理
	bool isDirty_ = false;
	std::function<void()> pendingDiscardAction_;
	bool openDiscardConfirm_ = false;

	// レイアウト
	float outlinerHeight_ = 260.0f;          // 縦並び時のアウトライナー高さ
	int renameEmitterIndex_ = -1;            // リネーム中のエミッター
	bool openRenamePopup_ = false;
	int addEmitterTemplate_ = 0;             // エミッター追加時のテンプレート
	char renameBuffer_[256] = {};
	char saveAsBuffer_[256] = {};
	char loadFilterBuffer_[128] = {};
	char addModuleFilterBuffer_[128] = {};
	char newParameterNameBuffer_[96] = "User.Value";
	int addModulePhaseFilter_ = 0;           // 0=All, 1=Spawn, 2=Update
	std::string addModuleSelection_;         // 選択中のモジュールID

	// プレビュー制御
	bool previewLooping_ = false;           // ループ再生フラグ
	float previewRepeatInterval_ = 2.0f;    // ループ間隔（秒）
	float previewElapsed_ = 0.0f;           // ループ用経過時間

	// トレイルプレビュー用ダミーターゲット設定
	bool enablePreviewTarget_ = false;       // ダミーターゲット有効フラグ
	float previewTargetSpeed_ = 3.0f;        // ダミー移動速度
	float previewTargetRadius_ = 4.0f;       // ダミー円運動半径
	float previewTargetTime_ = 0.0f;         // ダミー移動経過時間
	uint32_t previewSourceId_ = 0;           // プレビュー用ソースID
	Vector3 previewTargetPos_ = {};          // ダミーの現在位置
	Vector3 previewRestPosition_ = {};       // 動かし始めたときのエフェクト位置（軌道の中心）

	std::string effectPath_;
	std::string operationStatus_;
	bool operationSucceeded_ = true;
	char effectNameBuffer_[256] = {};
	char emitterNameBuffer_[256] = {};

	// 天球（Skydome）のカラー調整 (RGB)
	Vector4 skydomeColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};
} // namespace KCE
