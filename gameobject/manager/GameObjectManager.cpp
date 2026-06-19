#include "GameObjectManager.h"
#include "engine/gameobject/base/GameObject.h"
#include <algorithm>
#include "manager/editor/GameObjectEditor.h"

std::unique_ptr<GameObjectManager> GameObjectManager::instance_ = nullptr;

GameObjectManager* GameObjectManager::GetInstance()
{
	if (!instance_)
	{
		instance_.reset(new GameObjectManager());
	}
	return instance_.get();
}

bool GameObjectManager::HasInstance()
{
	return instance_ != nullptr;
}

void GameObjectManager::Initialize()
{
	gameObjects_.clear();
	dynamicGameObjects_.clear();
	cachedObject3dCommon_ = nullptr;
	cachedLightManager_ = nullptr;
}

void GameObjectManager::Finalize()
{
	// 安全のためエディタ側ポインタを全クリア
	if (GameObjectEditor::HasInstance())
	{
		for (auto* obj : gameObjects_)
		{
			GameObjectEditor::GetInstance()->OnGameObjectRemoved(obj);
		}
	}
	gameObjects_.clear();

	// 二重解放・再帰的呼び出し時のイテレータ破壊を防ぐため、一旦ローカル変数に移してからクリアする
	auto toDestroy = std::move(dynamicGameObjects_);
	toDestroy.clear();

	cachedObject3dCommon_ = nullptr;
	cachedLightManager_ = nullptr;
	instance_.reset();
}

void GameObjectManager::Register(GameObject* gameObject)
{
	if (gameObject)
	{
		auto it = std::find(gameObjects_.begin(), gameObjects_.end(), gameObject);
		if (it == gameObjects_.end())
		{
			gameObjects_.push_back(gameObject);
		}

		// システムポインタのキャッシュを試行
		if (!cachedObject3dCommon_ && gameObject->GetObject3dCommon())
		{
			cachedObject3dCommon_ = gameObject->GetObject3dCommon();
		}
		if (!cachedLightManager_ && gameObject->GetLightManager())
		{
			cachedLightManager_ = gameObject->GetLightManager();
		}
	}
}

void GameObjectManager::Unregister(GameObject* gameObject)
{
	if (gameObject)
	{
		// メモリ破棄前にエディタに通知して無効ポインタをクリア
		if (GameObjectEditor::HasInstance())
		{
			GameObjectEditor::GetInstance()->OnGameObjectRemoved(gameObject);
		}

		// 管理リストから削除
		gameObjects_.erase(std::remove(gameObjects_.begin(), gameObjects_.end(), gameObject), gameObjects_.end());

		// 二重解放・再帰的デストラクトを防ぐため、一時的に取り出してローカル変数で保持してから解放する
		std::unique_ptr<GameObject> toDelete = nullptr;
		for (auto it = dynamicGameObjects_.begin(); it != dynamicGameObjects_.end(); ++it)
		{
			if (it->get() == gameObject)
			{
				if (!it->get())
				{
					break;
				}
				toDelete = std::move(*it);
				dynamicGameObjects_.erase(it);
				break;
			}
		}
	}
}

void GameObjectManager::Update()
{
	// 安全ループ（Update内の Register/Unregister に備えてコピーを取る）
	auto tempObjects = gameObjects_;
	for (auto* obj : tempObjects)
	{
		auto it = std::find(gameObjects_.begin(), gameObjects_.end(), obj);
		if (it != gameObjects_.end() && obj->IsActive())
		{
			obj->Update();
		}
	}
}

void GameObjectManager::Draw3D(CameraManager* camera)
{
	for (auto* obj : gameObjects_)
	{
		if (obj->IsActive())
		{
			// Renderable3dが存在し、かつRenderingTypeがDeferredの場合は、DrawGBufferで描画されるためDraw3Dでは描画しない
			if (auto* renderable = obj->GetRenderable3d())
			{
				if (renderable->GetRenderingType() == RenderingType::Deferred)
				{
					continue;
				}
			}
			obj->Draw3D(camera);
		}
	}
}

void GameObjectManager::Draw2D()
{
	for (auto* obj : gameObjects_)
	{
		if (obj->IsActive())
		{
			obj->Draw2D();
		}
	}
}

void GameObjectManager::DrawShadow(Camera* camera)
{
	for (auto* obj : gameObjects_)
	{
		if (obj->IsActive())
		{
			obj->DrawShadow(camera);
		}
	}
}

void GameObjectManager::DrawGBuffer(CameraManager* camera)
{
	for (auto* obj : gameObjects_)
	{
		if (obj->IsActive())
		{
			// Renderable3dが存在し、かつRenderingTypeがDeferredのもののみ描画する
			if (auto* renderable = obj->GetRenderable3d())
			{
				if (renderable->GetRenderingType() != RenderingType::Deferred)
				{
					continue;
				}
			}
			else
			{
				// Renderable3dを持たないオブジェクトはGBufferパスでは何もしない
				continue;
			}
			obj->DrawGBuffer(camera);
		}
	}
}

GameObject* GameObjectManager::Find(const std::string& name) const
{
	for (auto* obj : gameObjects_)
	{
		if (obj->GetName() == name)
		{
			return obj;
		}
	}
	return nullptr;
}

std::vector<GameObject*> GameObjectManager::FindAll(const std::string& name) const
{
	std::vector<GameObject*> result;
	for (auto* obj : gameObjects_)
	{
		if (obj->GetName() == name)
		{
			result.push_back(obj);
		}
	}
	return result;
}

GameObject* GameObjectManager::CreateGameObject(const std::string& name, const std::string& tag)
{
	auto newObj = std::make_unique<GameObject>(tag);
	newObj->SetName(name);

	// キャッシュされているリソースがあれば初期化する
	if (cachedObject3dCommon_ && cachedLightManager_)
	{
		newObj->Initialize(cachedObject3dCommon_, cachedLightManager_);
	}

	GameObject* ptr = newObj.get();
	dynamicGameObjects_.push_back(std::move(newObj));
	Register(ptr);

	return ptr;
}
