#include "GameObjectManager.h"
#include "engine/gameobject/base/GameObject.h"
#include <algorithm>

std::unique_ptr<GameObjectManager> GameObjectManager::instance_ = nullptr;

GameObjectManager* GameObjectManager::GetInstance()
{
	if (!instance_)
	{
		instance_.reset(new GameObjectManager());
	}
	return instance_.get();
}

void GameObjectManager::Initialize()
{
	gameObjects_.clear();
}

void GameObjectManager::Finalize()
{
	gameObjects_.clear();
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
	}
}

void GameObjectManager::Unregister(GameObject* gameObject)
{
	if (gameObject)
	{
		gameObjects_.erase(std::remove(gameObjects_.begin(), gameObjects_.end(), gameObject), gameObjects_.end());
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

void GameObjectManager::DrawShadow()
{
	for (auto* obj : gameObjects_)
	{
		if (obj->IsActive())
		{
			obj->DrawShadow();
		}
	}
}

void GameObjectManager::DrawGBuffer()
{
	for (auto* obj : gameObjects_)
	{
		if (obj->IsActive())
		{
			obj->DrawGBuffer();
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
