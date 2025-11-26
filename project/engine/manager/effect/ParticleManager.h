#pragma once
#include <random>
#include <wrl.h>
#include <d3d12.h>
#include <list>
#include <unordered_map>

// system
#include "ParticlePipelineManager.h"
#include "manager/scene/CameraManager.h"
#include "graphics/3d/Model.h"
#include "effects/particle/ParticleEmitter.h"

// 前方宣言
class DirectXCommon;
class SrvManager;

/**
 * @brief 頂点形状の種類
 */
enum class VertexShape
{
	Plane, // 平面
	Ring   // リング
};

/**
 * @brief パーティクルマネージャークラス
 * @details パーティクルシステムを管理するシングルトンクラス
 *          ParticlePipelineManagerと連携してブレンドモード別の描画を行う
 *          エミッターの登録・登録解除・更新・描画を管理する
 */
class ParticleManager
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return ParticleManagerのインスタンス
	 */
	static ParticleManager* GetInstance();

	/**
	 * @brief シングルトンの解放
	 */
	static void Finalize();

	/**
	 * @brief 初期化処理
	 * @param dxCommon DirectXCommonへのポインタ
	 * @param srvManager SRVマネージャーへのポインタ
	 */
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/**
	 * @brief 更新処理
	 * @param camera カメラマネージャーへのポインタ
	 */
	void Update(CameraManager* camera);

	/**
	 * @brief 描画処理
	 * @details 登録されたすべてのエミッターを描画する
	 */
	void Draw();

	/**
	 * @brief エミッターの登録
	 * @param name エミッターの名前
	 * @param emitter 登録するエミッターへのポインタ
	 */
	void RegisterEmitter(const std::string& name, ParticleEmitter* emitter);

	/**
	 * @brief エミッターの登録解除
	 * @param name 登録解除するエミッターの名前
	 */
	void UnregisterEmitter(const std::string& name);
	
	/**
	 * @brief DirectXCommonの取得
	 * @return DirectXCommonへのポインタ
	 */
	DirectXCommon* GetDxCommon() { return dxCommon_; }

	/**
	 * @brief SRVマネージャーの取得
	 * @return SRVマネージャーへのポインタ
	 */
	SrvManager* GetSrvManager() { return srvManager_; }

private: // メンバ変数
	/*--------------[ ポインタ ]-----------------*/

	DirectXCommon* dxCommon_ = nullptr;  // DirectXCommonへのポインタ
	SrvManager* srvManager_ = nullptr;   // SRVマネージャーへのポインタ
	Model* model_ = nullptr;             // モデルへのポインタ
	// パイプラインマネージャー
	std::unique_ptr<ParticlePipelineManager> pipelineManager_ = nullptr;

	/*--------------[ コンテナ ]-----------------*/

	// エミッターのリスト（名前 -> エミッター）
	std::unordered_map<std::string, ParticleEmitter*> emitters_;

private:
	/*========[ シングルトン ]========*/
	static ParticleManager* instance_;   // シングルトンインスタンス
	// コピー禁止
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager& rhs) = delete;
	ParticleManager& operator=(const ParticleManager& rhs) = delete;
};

