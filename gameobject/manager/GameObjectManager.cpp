#include "GameObjectManager.h"
#include "engine/gameobject/base/GameObject.h"
#include <algorithm>
#include "manager/scene/CameraManager.h"
#include "manager/editor/GameObjectEditor.h"

namespace KCE
{
std::unique_ptr<GameObjectManager> GameObjectManager::instance_ = nullptr;

GameObjectManager* GameObjectManager::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<GameObjectManager>();
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
		if (it != gameObjects_.end() && obj->IsActive() && !obj->IsPendingDestroy())
		{
			obj->Update();
		}
	}

	// 破棄保留中のオブジェクトを安全に解放
	ClearPendingDestroyObjects();
}

void GameObjectManager::Draw3D(CameraManager* camera)
{
	for (auto* obj : gameObjects_)
	{
		if (obj->IsActive())
		{
			// 現在描いているビューの対象でなければ飛ばす
			if (!IsVisibleInLayerMask(obj->GetRenderLayer(), renderLayerMask_))
			{
				continue;
			}

			// Renderable3dが存在し、かつRenderingTypeがDeferredの場合は、DrawGBufferで描画されるためDraw3Dでは描画しない
			if (auto* renderable = obj->GetRenderable3d())
			{
				if (renderable->GetRenderQueue() != RenderQueue::Opaque || renderable->GetRenderingType() == RenderingType::Deferred)
				{
					continue;
				}
			}
			obj->Draw3D(camera);
		}
	}
}

void GameObjectManager::DrawTransparent(CameraManager* camera, const std::vector<Object3d*>& sceneObjects)
{
	if (!camera || !camera->GetActiveCamera())
	{
		return;
	}
	const Vector3 cameraPosition = camera->GetActiveCamera()->GetTranslate();
	struct Entry
	{
		IRenderable3d* object;
		float distanceSquared;
	};
	std::vector<Entry> entries;
	std::vector<GameObject*> roots;
	for (auto* obj : gameObjects_)
	{
		while (obj->GetParent())
		{
			obj = obj->GetParent();
		}
		if (std::find(roots.begin(), roots.end(), obj) == roots.end())
		{
			roots.push_back(obj);
		}
	}
	// 親を先に更新し、未登録の子も一度だけ収集して全体でソートする。
	const auto collect = [&](const auto& self, GameObject* obj) -> void
	{
		if (!obj->IsActive())
		{
			return;
		}
		auto* renderable = obj->GetRenderable3d();
		if (renderable)
		{
			if (IsVisibleInLayerMask(obj->GetRenderLayer(), renderLayerMask_) &&
				renderable->GetRenderQueue() == RenderQueue::Transparent)
			{
				const auto world = obj->GetWorldMatrix();
				const Vector3 offset{ world.m[3][0] - cameraPosition.x, world.m[3][1] - cameraPosition.y, world.m[3][2] - cameraPosition.z };
				entries.push_back({ renderable, offset.x * offset.x + offset.y * offset.y + offset.z * offset.z });
			}
		}
		for (const auto& [name, child] : obj->GetChildren())
		{
			if (child)
			{
				self(self, child.get());
			}
		}
	};
	for (auto* root : roots)
	{
		collect(collect, root);
	}
	// シーンへ直接登録されたオブジェクトも同じキューでソートする。
	for (auto* object : sceneObjects)
	{
		if (!object || object->GetRenderQueue() != RenderQueue::Transparent ||
			std::any_of(entries.begin(), entries.end(), [object](const Entry& entry) { return entry.object == object; }))
		{
			continue;
		}
		const auto world = object->GetWorldMatrix();
		const Vector3 offset{ world.m[3][0] - cameraPosition.x, world.m[3][1] - cameraPosition.y, world.m[3][2] - cameraPosition.z };
		entries.push_back({ object, offset.x * offset.x + offset.y * offset.y + offset.z * offset.z });
	}
	std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b)
	{
		return a.distanceSquared > b.distanceSquared;
	});
	for (const auto& entry : entries)
	{
		// 子の再帰描画を避け、収集したオブジェクト単位の順序を維持する。
		entry.object->Draw();
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
			// 現在描いているビューの対象でなければ飛ばす
			if (!IsVisibleInLayerMask(obj->GetRenderLayer(), renderLayerMask_))
			{
				continue;
			}

			// Renderable3dが存在し、かつRenderingTypeがDeferredのもののみ描画する
			if (auto* renderable = obj->GetRenderable3d())
			{
				if (renderable->GetRenderQueue() != RenderQueue::Opaque || renderable->GetRenderingType() != RenderingType::Deferred)
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

GameObject* GameObjectManager::FindByGuid(const Guid& guid) const
{
	if (!guid.IsValid())
	{
		return nullptr;
	}

	for (auto* obj : gameObjects_)
	{
		if (obj->GetGuid() == guid)
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

GameObject* GameObjectManager::FindWithTag(const std::string& tag) const
{
	for (auto* obj : gameObjects_)
	{
		if (obj && obj->GetTag() == tag)
		{
			return obj;
		}
	}
	return nullptr;
}

std::vector<GameObject*> GameObjectManager::FindAllWithTag(const std::string& tag) const
{
	std::vector<GameObject*> result;
	for (auto* obj : gameObjects_)
	{
		if (obj && obj->GetTag() == tag)
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

void GameObjectManager::ClearPendingDestroyObjects()
{
	std::vector<GameObject*> toDestroy;
	for (auto* obj : gameObjects_)
	{
		if (obj && obj->IsPendingDestroy())
		{
			toDestroy.push_back(obj);
		}
	}

	for (auto* obj : toDestroy)
	{
		Unregister(obj);
	}
}
} // namespace KCE
