#pragma once
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
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief 更新（ImGui描画）
	 */
	void Update(CameraManager* camera);

	/**
	 * @brief プレビュー描画
	 */
	void Draw();

	/**
	 * @brief エディタを表示/非表示
	 */
	void SetVisible(bool visible) { isVisible_ = visible; }
	bool IsVisible() const { return isVisible_; }
	void ToggleVisible() { isVisible_ = !isVisible_; }

	/**
	 * @brief 新規エフェクト作成
	 */
	void NewEffect();

	/**
	 * @brief エフェクトを読み込み
	 */
	void LoadEffect(const std::string& path);

	/**
	 * @brief エフェクトを保存
	 */
	void SaveEffect(const std::string& path);

	/**
	 * @brief 編集中のエフェクトを取得
	 */
	ParticleEffect* GetCurrentEffect() const { return currentEffect_; }

private:
	void DrawMenuBar();
	void DrawEffectPanel();
	void DrawEmitterPanel();
	void DrawModulePanel();
	void DrawRendererPanel();
	void DrawPreviewPanel();
	void DrawCurveEditor();
	void DrawGradientEditor();
	void DrawModuleProperties(class IModule* module);

	void AddEmitterDialog();
	void AddModuleDialog(ParticleEmitter* emitter);

private:
	ParticleEffect* currentEffect_ = nullptr;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	bool isVisible_ = false;
	bool showAddEmitterDialog_ = false;
	bool showAddModuleDialog_ = false;
	int selectedEmitterIndex_ = -1;
	int selectedModuleIndex_ = -1;

	std::string effectPath_;
	char effectNameBuffer_[256] = {};
	char emitterNameBuffer_[256] = {};
};
