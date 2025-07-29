#include "ParticleManager.h"

#include <dxcapi.h>
#include <numbers>

// system
#include "manager/graphics/TextureManager.h"
#include "manager/graphics/ModelManager.h"
#include "base/DirectXCommon.h"
#include "manager/system/SrvManager.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

ParticleManager* ParticleManager::instance_ = nullptr;

ParticleManager* ParticleManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new ParticleManager();
	}
	return instance_;
}

void ParticleManager::Finalize()
{
	if (instance_ != nullptr)
	{
		// パイプラインマネージャーの解放
		instance_->pipelineManager_.reset();


		
		delete instance_;
		instance_ = nullptr;
	}
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	//引数をメンバ変数に記録
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	//パイプラインマネージャーの初期化
	pipelineManager_ = std::make_unique<ParticlePipelineManager>();
	pipelineManager_->Initialize(dxCommon_);

	//　エミッターの初期化
	emitters_.clear();
}

void ParticleManager::Update(CameraManager* camera)
{
#ifdef _DEBUG
	/*--------------[ ImGui ]-----------------*/
	ImGui::Begin("ParticleManager");

	for (auto& emitter : emitters_)
	{
		if (ImGui::CollapsingHeader(emitter.first.c_str()))
		{
			emitter.second->DrawImGui();
		}
	}

	ImGui::End();
#endif

	for (auto& emitter : emitters_)
	{
		emitter.second->Update(camera);
	}

}

void ParticleManager::Draw()
{
	/*--------------[ ルートシグネチャの設定 ]-----------------*/

	dxCommon_->GetCommandList()->SetGraphicsRootSignature(pipelineManager_->GetRootSignature());

	/*--------------[ パイプラインステートの設定 ]-----------------*/

	dxCommon_->GetCommandList()->SetPipelineState(pipelineManager_->GetPipelineState());

	/*--------------[ プリミティブトポロジーの設定 ]-----------------*/

	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	/*--------------[ パーティクルの描画 ]-----------------*/

	for (auto& emitter : emitters_)
	{
		//NULLチェック
		if (!emitter.second) { continue; }
		emitter.second->Draw(dxCommon_, srvManager_);
	}
	
}

void ParticleManager::RegisterEmitter(const std::string& name, ParticleEmitter* emitter)
{
	// 既に存在してたらエラー
	if (emitters_.find(name) != emitters_.end())
	{
		assert(0 && "Emitter already registered");
	}
	//登録
	emitters_[name] = emitter;
}

void ParticleManager::UnregisterEmitter(const std::string& name)
{
	// 既に存在してたらエラー
	if (emitters_.find(name) == emitters_.end())
	{
		assert(0 && "Emitter not found");
	}
	//登録解除
	emitters_.erase(name);
}
