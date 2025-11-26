#include "ModelManager.h"

// シングルトンインスタンスの実体
ModelManager* ModelManager::instance_ = nullptr;

ModelManager* ModelManager::GetInstance()
{
	// インスタンスが存在しない場合は生成
	if (instance_ == nullptr)
	{
		instance_ = new ModelManager();
	}
	return instance_;
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
	// モデル共通設定の初期化
	modelCommon_ = new ModelCommon();
	modelCommon_->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
	if (instance_ != nullptr)
	{
		// リソースの解放
		delete modelCommon_;
		delete instance_;
		instance_ = nullptr;
	}
}

void ModelManager::LoadModel(const std::string& filePath, const std::string& modelType)
{
	// 読み込み済みモデルを検索
	if(models_.contains(filePath))
	{
		// 読み込み済みなら早期リターン
		return;
	}

	// モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->Initialize(modelCommon_, "Resources/models", filePath,modelType);

	// モデルをmapコンテナに格納する（キャッシング）
	models_.insert(std::make_pair(filePath, std::move(model)));
}

Model* ModelManager::FindModel(const std::string& filePath)
{
	// 読み込み済みモデルを検索
	if (models_.contains(filePath))
	{
		// 読み込み済みならモデルを返す
		return models_.at(filePath).get();
	}

	// ファイル名一致なし
	return nullptr;
}
