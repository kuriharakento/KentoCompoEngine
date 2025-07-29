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

//前方宣言
class DirectXCommon;
class SrvManager;

enum class VertexShape
{
	Plane,
	Ring
};

class ParticleManager
{
public:
	//シングルトンのインスタンスを取得
	static ParticleManager* GetInstance();
	//シングルトンの解放
	static void Finalize();
	///初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	///更新
	void Update(CameraManager* camera);
	///描画
	void Draw();
	//エミッターの登録
	void RegisterEmitter(const std::string& name, ParticleEmitter* emitter);
	void UnregisterEmitter(const std::string& name);
	
	DirectXCommon* GetDxCommon() { return dxCommon_; }
	SrvManager* GetSrvManager() { return srvManager_; }
private: //メンバ変数
	/*--------------[ ポインタ ]-----------------*/

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Model* model_ = nullptr;
	//パイプラインマネージャー
	std::unique_ptr<ParticlePipelineManager> pipelineManager_ = nullptr;

	/*--------------[ コンテナ ]-----------------*/

	//エミッターのリスト
	std::unordered_map<std::string, ParticleEmitter*> emitters_;

private:
	/*========[ シングルトン ]========*/
	static ParticleManager* instance_;
	//コピー禁止
	ParticleManager() = default;
	~ParticleManager() = default;
	ParticleManager(const ParticleManager& rhs) = delete;
	ParticleManager& operator=(const ParticleManager& rhs) = delete;
};

