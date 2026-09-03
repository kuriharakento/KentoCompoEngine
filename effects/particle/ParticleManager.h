#pragma once
/**
 * @file ParticleManager.h
 * @brief パーティクルマネージャー
 * 
 * エミッターとエフェクトの一元管理。
 * Play() APIでエフェクトを簡単に再生可能。
 */
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "ParticleEmitter.h"
#include "effects/particle/diagnostics/ParticleDiagnostics.h"

namespace KCE
{
class DirectXCommon;
class SrvManager;
class CameraManager;
class ParticlePipelineManager;
class ParticleEffect;
class GPUSimulator;

/**
 * @brief パーティクルマネージャー
 * 
 * エミッターとエフェクトの両方を管理する
 */
class ParticleManager
{
public:
	static ParticleManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Finalize();
	void Update(CameraManager* camera);
	void Draw();

	/**
	 * @brief ImGuiでデバッグ情報を表示
	 */
	void DrawImGui();

	//===== エフェクトのロード（推奨API）=====//

	/**
	 * @brief エフェクトをJSONからロードして登録（非アクティブ状態）
	 * 
	 * ロード後にエフェクトを取得して設定変更し、Play()で再生できる。
	 * @param name エフェクト名（GetEffectで取得に使用）
	 * @param jsonPath JSONファイルパス
	 * @return ロードしたエフェクト。失敗時nullptr
	 */
	ParticleEffect* Load(const std::string& name, const std::string& jsonPath);

	/**
	 * @brief 空のエフェクトを作成して登録（非アクティブ状態）
	 * @param name エフェクト名
	 * @return 作成したエフェクト
	 */
	ParticleEffect* CreateEmpty(const std::string& name);

	/**
	 * @brief エフェクトが登録済みかチェック
	 * @param name エフェクト名
	 * @return 登録済みならtrue
	 */
	bool HasEffect(const std::string& name) const;

	//===== エフェクト定義の管理（後方互換）=====//

	/**
	 * @brief エフェクト定義を事前読み込み（非推奨：Load()を使用）
	 * @param name エフェクト名（Play時に使用）
	 * @param jsonPath JSONファイルパス
	 */
	void LoadEffectDefinition(const std::string& name, const std::string& jsonPath);

	//===== 再生API =====//

	/**
	 * @brief エフェクトを再生（メインAPI）
	 * 
	 * 既存のエフェクトがあればそれを再生、なければ定義からロードして再生。
	 * @param effectName 登録済みエフェクト名
	 * @param position 再生位置
	 * @return 再生中のエフェクト（制御用）。登録がなければnullptr
	 */
	ParticleEffect* Play(const std::string& effectName, const Vector3& position = {});

	/**
	 * @brief エフェクトを再生（追従ターゲット付き）
	 * @param effectName 登録済みエフェクト名
	 * @param followTarget 追従対象
	 * @return 再生中のエフェクト。登録がなければnullptr
	 */
	ParticleEffect* Play(const std::string& effectName, Transform* followTarget);

	/**
	 * @brief エフェクトの事前生成（プールウォームアップ）
	 * @param effectName ロード済みエフェクト名
	 * @param count 事前生成数
	 */
	void Warmup(const std::string& effectName, size_t count);

	//===== 手動管理API =====//

	/**
	 * @brief エフェクトを手動で追加
	 */
	void AddEffect(std::unique_ptr<ParticleEffect> effect);

	/**
	 * @brief 名前でエフェクトを取得
	 */
	ParticleEffect* GetEffect(const std::string& name);

	/**
	 * @brief エミッターを直接追加（後方互換用）
	 */
	void AddEmitter(std::unique_ptr<ParticleEmitter> emitter);

	/**
	 * @brief エミッターを取得
	 */
	ParticleEmitter* GetEmitter(const std::string& name);

	/**
	 * @brief 全エフェクト停止
	 */
	void StopAll();

	/**
	 * @brief 全てクリア
	 */
	void Clear();

	/**
	 * @brief エフェクトを手動で削除
	 */
	void RemoveEffect(ParticleEffect* effect);

	/** プール中のEffectをGPU fence完了後に実破棄する。 */
	void PurgeEffectPools();
	size_t GetPooledEffectCount() const;

	/**
	 * @brief レンダラーを遅延破棄キュー（ゴミ箱）に追加
	 * @param renderer 破棄するレンダラー
	 */
	void AddRendererToTrashBin(std::unique_ptr<IRenderer> renderer);

	/** GPUが旧bufferを参照している間、Simulatorの破棄を次フレームまで遅延する。 */
	void AddSimulatorToTrashBin(std::unique_ptr<GPUSimulator> simulator);

	//===== アクセサ =====//

	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	SrvManager* GetSrvManager() const { return srvManager_; }
	ParticlePipelineManager* GetPipelineManager() const { return pipelineManager_.get(); }

	/**
	 * @brief 登録エフェクト数を取得
	 */
	size_t GetEffectCount() const { return effects_.size(); }

	/**
	 * @brief インデックスでエフェクトを取得
	 * @param index エフェクトインデックス
	 * @return エフェクト。範囲外ならnullptr
	 */
	ParticleEffect* GetEffect(size_t index);
	const ParticleEffect* GetEffect(size_t index) const;

	/**
	 * @brief 名前でエフェクトを削除
	 * @param name エフェクト名
	 * @return 削除したらtrue
	 */
	bool RemoveEffect(const std::string& name);

private:
	ParticleManager() = default;
	~ParticleManager();
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;

	void RemoveFinishedEffects();

private:
	// エフェクト定義（名前 → JSONパス）
	std::unordered_map<std::string, std::string> effectDefinitions_;

	// アクティブなエフェクト
	std::vector<std::unique_ptr<ParticleEffect>> effects_;

	// 非アクティブなエフェクトプール（名前 → プールリスト）
	std::unordered_map<std::string, std::vector<std::unique_ptr<ParticleEffect>>> effectPools_;

	// 遅延破棄するレンダラーリスト（GPU使用中のリソース安全破棄用）
	struct RetiredRenderer { uint64_t fenceValue; std::unique_ptr<IRenderer> resource; };
	struct RetiredSimulator { uint64_t fenceValue; std::unique_ptr<GPUSimulator> resource; };
	struct RetiredEffect { uint64_t fenceValue; std::unique_ptr<ParticleEffect> resource; };
	std::vector<RetiredRenderer> rendererTrashBin_;
	std::vector<RetiredSimulator> simulatorTrashBin_;
	std::vector<RetiredEffect> effectTrashBin_;

	// 直接追加されたエミッター（後方互換用）
	std::vector<std::unique_ptr<ParticleEmitter>> emitters_;

	std::unique_ptr<ParticlePipelineManager> pipelineManager_;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
};
} // namespace KCE
