#include "ModelManager.h"

#include <algorithm>

#include "manager/graphics/TextureManager.h"

namespace KCE
{
// シングルトンインスタンスの実体
std::unique_ptr<ModelManager> ModelManager::instance_ = nullptr;

ModelManager* ModelManager::GetInstance()
{
	// インスタンスが存在しない場合は生成
	if (instance_ == nullptr)
	{
		instance_ = std::make_unique<ModelManager>();
	}
	return instance_.get();
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
	// モデル共通設定の初期化
	modelCommon_ = std::make_unique<ModelCommon>();
	modelCommon_->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
	instance_.reset();
}

void ModelManager::LoadModel(const std::string& filePath, const std::string& modelType, ResourceLifetime lifetime)
{
	// 読み込み済みモデルを検索
	if (models_.contains(filePath))
	{
		if (lifetime == ResourceLifetime::Resident)
		{
			models_.at(filePath).lifetime = ResourceLifetime::Resident;
			for (const auto& material : models_.at(filePath).model->GetModelData().materials)
			{
				TextureManager::GetInstance()->MarkResident(material.textureFilePath);
			}
		}
		return;
	}
	if (failedModels_.contains(filePath + modelType))
	{
		return;
	}

	// モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	if (!model->Initialize(modelCommon_.get(), "Resources/models", filePath, modelType))
	{
		// 読み込みに失敗したモデルは登録せず、FindModelでnullptrを返せるようにする。
		failedModels_.insert(filePath + modelType);
		return;
	}

	// モデルをmapコンテナに格納する（キャッシング）
	knownModelTypes_[filePath] = modelType;
	Model* loadedModel = model.get();
	models_.insert(std::make_pair(filePath, ModelEntry{ std::move(model), lifetime }));
	if (lifetime == ResourceLifetime::Resident)
	{
		for (const auto& material : loadedModel->GetModelData().materials)
		{
			TextureManager::GetInstance()->MarkResident(material.textureFilePath);
		}
	}
}

void ModelManager::ReleaseSceneResources()
{
	std::erase_if(models_, [](const auto& item)
	{
		return item.second.lifetime == ResourceLifetime::Scene;
	});
}

size_t ModelManager::GetResidentModelCount() const
{
	return static_cast<size_t>(std::count_if(models_.begin(), models_.end(), [](const auto& item)
	{
		return item.second.lifetime == ResourceLifetime::Resident;
	}));
}

size_t ModelManager::GetSceneModelCount() const
{
	return models_.size() - GetResidentModelCount();
}

Model* ModelManager::FindModel(const std::string& filePath)
{
	// 読み込み済みモデルを検索
	if (models_.contains(filePath))
	{
		// 読み込み済みならモデルを返す
		return models_.at(filePath).model.get();
	}

	// 解放後にパスで引かれたら、前回と同じ形式でシーン素材として読み直す。
	auto type = knownModelTypes_.find(filePath);
	LoadModel(filePath, type != knownModelTypes_.end() ? type->second : ".obj", ResourceLifetime::Scene);
	auto loaded = models_.find(filePath);
	return loaded != models_.end() ? loaded->second.model.get() : nullptr;
}
} // namespace KCE
