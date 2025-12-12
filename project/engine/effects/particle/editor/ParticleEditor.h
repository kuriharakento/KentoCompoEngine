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

class ParticleEffect;
class ParticleEmitter;
class DirectXCommon;
class SrvManager;
class CameraManager;

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
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief 更新（ImGui描画）
	 * @param camera カメラマネージャー
	 */
	void Update(CameraManager* camera);

	/**
	 * @brief デバッグ表示描画（エミッター位置、パーティクル位置をライン描画）
	 */
	void DrawDebug();

	/**
	 * @brief エディタの表示状態を設定
	 * @param visible 表示するか
	 */
	void SetVisible(bool visible) { isVisible_ = visible; }

	/**
	 * @brief エディタが表示中か判定
	 * @return 表示中の場合true
	 */
	bool IsVisible() const { return isVisible_; }

	/**
	 * @brief エディタの表示状態を切り替え
	 */
	void ToggleVisible() { isVisible_ = !isVisible_; }

	/**
	 * @brief 新規エフェクトを作成
	 */
	void NewEffect();

	/**
	 * @brief エフェクトを読み込み
	 * @param path JSONファイルパス
	 */
	void LoadEffect(const std::string& path);

	/**
	 * @brief エフェクトを保存
	 * @param path 保存先JSONファイルパス
	 */
	void SaveEffect(const std::string& path);

	/**
	 * @brief 編集中のエフェクトを取得
	 * @return 現在のエフェクト（nullptrの可能性あり）
	 */
	ParticleEffect* GetCurrentEffect() const { return currentEffect_; }

private:
	/**
	 * @brief メニューバーを描画
	 */
	void DrawMenuBar();

	/**
	 * @brief エフェクト設定パネルを描画
	 */
	void DrawEffectPanel();

	/**
	 * @brief エミッター設定パネルを描画
	 */
	void DrawEmitterPanel();

	/**
	 * @brief モジュール設定パネルを描画
	 */
	void DrawModulePanel();

	/**
	 * @brief レンダラー設定パネルを描画
	 */
	void DrawRendererPanel();

	/**
	 * @brief プレビューパネルを描画
	 */
	void DrawPreviewPanel();

	/**
	 * @brief カーブエディタを描画
	 */
	void DrawCurveEditor();

	/**
	 * @brief グラデーションエディタを描画
	 */
	void DrawGradientEditor();

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

private:
	ParticleEffect* currentEffect_ = nullptr;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	bool isVisible_ = false;
	bool showDebug_ = true;  // デバッグ表示ON/OFF
	bool showAddEmitterDialog_ = false;
	bool showAddModuleDialog_ = false;
	int selectedEmitterIndex_ = -1;
	int selectedModuleIndex_ = -1;

	std::string effectPath_;
	char effectNameBuffer_[256] = {};
	char emitterNameBuffer_[256] = {};
};

