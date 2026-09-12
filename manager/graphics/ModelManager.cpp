#include "ModelManager.h"

#include <algorithm>

#include "base/JobSystem.h"
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
	if (IsAlreadyHandled(filePath, modelType, lifetime))
	{
		return;
	}

	// モデルの生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	if (!model->Initialize(modelCommon_.get(), kModelDirectory, filePath, modelType))
	{
		// 読み込みに失敗したモデルは登録せず、FindModelでnullptrを返せるようにする。
		failedModels_.insert(filePath + modelType);
		return;
	}

	RegisterModel(filePath, modelType, std::move(model), lifetime);
}

void ModelManager::LoadModels(const std::vector<ModelRequest>& requests, ResourceLifetime lifetime, JobSystem* jobSystem)
{
	// 解析が要るものだけ残す。読み込み済みと、リスト内の重複はここで落とす
	struct PendingModel
	{
		// requests の要素を指す。この関数の中でしか使わない
		const ModelRequest* request = nullptr;
		Model::ParsedModel parsed;
		bool isParsed = false;
	};
	std::vector<PendingModel> pendingModels;
	// 途中で伸びると解析中の要素が動くので、先に確保しきっておく
	pendingModels.reserve(requests.size());
	std::unordered_set<std::string> queuedPaths;
	for (const auto& request : requests)
	{
		if (IsAlreadyHandled(request.filePath, request.modelType, lifetime) || !queuedPaths.insert(request.filePath).second)
		{
			continue;
		}
		pendingModels.emplace_back().request = &request;
	}

	// ファイルの解析だけ並べる。GPU とマネージャーはここでは触らない
	const auto parse = [&pendingModels](size_t index)
	{
		PendingModel& pending = pendingModels[index];
		pending.isParsed = Model::Parse(kModelDirectory, pending.request->filePath, pending.request->modelType, pending.parsed);
	};
	if (jobSystem)
	{
		jobSystem->ParallelFor(pendingModels.size(), parse);
	}
	else
	{
		for (size_t index = 0; index < pendingModels.size(); ++index)
		{
			parse(index);
		}
	}

	// モデルが使うテクスチャも先にまとめて並べて読む。Initialize の中の LoadTexture は読み込み済みで素通りになる
	std::vector<std::string> texturePaths;
	for (const PendingModel& pending : pendingModels)
	{
		if (!pending.isParsed)
		{
			continue;
		}
		for (const auto& material : pending.parsed.modelData.materials)
		{
			if (!material.textureFilePath.empty())
			{
				texturePaths.push_back(material.textureFilePath);
			}
		}
	}
	TextureManager::GetInstance()->LoadTextures(texturePaths, ResourceLifetime::Scene, jobSystem);

	// GPU リソースは並べた順に作る
	for (PendingModel& pending : pendingModels)
	{
		std::unique_ptr<Model> model = std::make_unique<Model>();
		if (!pending.isParsed || !model->Initialize(modelCommon_.get(), std::move(pending.parsed)))
		{
			// 読み込みに失敗したモデルは登録せず、FindModelでnullptrを返せるようにする。
			failedModels_.insert(pending.request->filePath + pending.request->modelType);
			continue;
		}
		RegisterModel(pending.request->filePath, pending.request->modelType, std::move(model), lifetime);
	}
}

bool ModelManager::IsAlreadyHandled(const std::string& filePath, const std::string& modelType, ResourceLifetime lifetime)
{
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
		return true;
	}
	return failedModels_.contains(filePath + modelType);
}

void ModelManager::RegisterModel(const std::string& filePath, const std::string& modelType, std::unique_ptr<Model> model, ResourceLifetime lifetime)
{
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
