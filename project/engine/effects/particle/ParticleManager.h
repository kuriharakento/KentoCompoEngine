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

class DirectXCommon;
class SrvManager;
class CameraManager;
class ParticlePipelineManager;
class ParticleEffect;

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

	//===== エフェクト定義の管理 =====//

	/**
	 * @brief エフェクト定義を事前読み込み
	 * @param name エフェクト名（Play時に使用）
	 * @param jsonPath JSONファイルパス
	 */
	void LoadEffectDefinition(const std::string& name, const std::string& jsonPath);

	//===== シンプルAPI =====//

	/**
	 * @brief エフェクトを再生（メインAPI）
	 * @param effectName 登録済みエフェクト名
	 * @param position 再生位置
	 * @return 再生中のエフェクト（制御用）
	 */
	ParticleEffect* Play(const std::string& effectName, const Vector3& position = {});

	/**
	 * @brief エフェクトを再生（追従ターゲット付き）
	 * @param effectName 登録済みエフェクト名
	 * @param followTarget 追従対象
	 * @return 再生中のエフェクト
	 */
	ParticleEffect* Play(const std::string& effectName, Transform* followTarget);

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

	//===== アクセサ =====//

	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	SrvManager* GetSrvManager() const { return srvManager_; }
	ParticlePipelineManager* GetPipelineManager() const { return pipelineManager_.get(); }

private:
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager&) = delete;
	ParticleManager& operator=(const ParticleManager&) = delete;

	void RemoveFinishedEffects();

private:
	// エフェクト定義（名前 → JSONパス）
	std::unordered_map<std::string, std::string> effectDefinitions_;

	// アクティブなエフェクト
	std::vector<std::unique_ptr<ParticleEffect>> effects_;

	// 直接追加されたエミッター（後方互換用）
	std::vector<std::unique_ptr<ParticleEmitter>> emitters_;

	std::unique_ptr<ParticlePipelineManager> pipelineManager_;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
};
