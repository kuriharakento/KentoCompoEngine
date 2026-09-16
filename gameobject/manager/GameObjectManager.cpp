#include "GameObjectManager.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/component/base/ComponentFactory.h"
#include "jsonEditor/JsonEditableBase.h"
#include "jsonEditor/JsonSerialization.h"
#include "base/Logger.h"
#include "base/PathManager.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include "manager/scene/CameraManager.h"
#include "manager/editor/GameObjectEditor.h"
#include "base/Camera.h"
#include "graphics/3d/IRenderable3d.h"
#include "math/AABB.h"
#include "time/TimeManager.h"

namespace KCE
{
namespace
{
constexpr int kPrefabVersion = 1;
constexpr uint32_t kAabbCornerCount = 8;

/**
 * @brief ローカルの AABB が、行列で送ったクリップ空間で完全に外にあるか
 * @details 8つの角が全部、同じ面（左右上下・手前・奥）の外にあれば外とする。
 *          角がばらばらの面の外にあるだけなら中に入っているかもしれないので、外とは言わない（描きすぎる側に倒す）。
 *          行は行ベクトル（v * M）、深度は DirectX の 0〜w
 */
bool IsAabbOutsideClip(const AABB& bounds, const Matrix4x4& localToClip)
{
	// 面ごとに「全部の角がこの面の外か」を持つ。左・右・下・上・手前・奥
	bool outside[6] = { true, true, true, true, true, true };
	for (uint32_t i = 0; i < kAabbCornerCount; ++i)
	{
		const float x = (i & 1) ? bounds.max_.x : bounds.min_.x;
		const float y = (i & 2) ? bounds.max_.y : bounds.min_.y;
		const float z = (i & 4) ? bounds.max_.z : bounds.min_.z;
		const float cx = x * localToClip.m[0][0] + y * localToClip.m[1][0] + z * localToClip.m[2][0] + localToClip.m[3][0];
		const float cy = x * localToClip.m[0][1] + y * localToClip.m[1][1] + z * localToClip.m[2][1] + localToClip.m[3][1];
		const float cz = x * localToClip.m[0][2] + y * localToClip.m[1][2] + z * localToClip.m[2][2] + localToClip.m[3][2];
		const float cw = x * localToClip.m[0][3] + y * localToClip.m[1][3] + z * localToClip.m[2][3] + localToClip.m[3][3];
		outside[0] = outside[0] && cx < -cw;
		outside[1] = outside[1] && cx > cw;
		outside[2] = outside[2] && cy < -cw;
		outside[3] = outside[3] && cy > cw;
		outside[4] = outside[4] && cz < 0.0f;
		outside[5] = outside[5] && cz > cw;
	}
	return outside[0] || outside[1] || outside[2] || outside[3] || outside[4] || outside[5];
}

std::unique_ptr<GameObject> DeserializePrefabNode(
	const nlohmann::json& node, Object3dCommon* object3dCommon, LightManager* lightManager)
{
	if (!node.is_object() || node.value("version", 0) != kPrefabVersion ||
		!node.contains("name") || !node["name"].is_string() || node["name"].get_ref<const std::string&>().empty() ||
		!node.contains("tag") || !node["tag"].is_string() || node["tag"].get_ref<const std::string&>().empty() ||
		!node.contains("transform") || !node["transform"].is_object() ||
		!node.contains("components") || !node["components"].is_array() ||
		!node.contains("children") || !node["children"].is_array())
	{
		return nullptr;
	}

	auto object = std::make_unique<GameObject>(node["tag"].get<std::string>());
	object->SetName(node["name"].get<std::string>());
	object->Initialize(object3dCommon, lightManager);
	// 登録名（transform_）に頼らず、Transform として読む。形が違えば例外になり、LoadPrefab が受ける
	const Transform transform = node["transform"].get<Transform>();
	object->SetScale(transform.scale);
	object->SetRotation(transform.rotate);
	object->SetPosition(transform.translate);
	if (node.contains("model"))
	{
		if (!node["model"].is_string() || node["model"].get_ref<const std::string&>().empty())
		{
			return nullptr;
		}
		object->SetModel(node["model"].get<std::string>());
	}

	for (const auto& componentNode : node["components"])
	{
		if (!componentNode.is_object() || !componentNode.contains("type") || !componentNode["type"].is_string() ||
			!componentNode.contains("enabled") || !componentNode["enabled"].is_boolean() ||
			!componentNode.contains("fields") || !componentNode["fields"].is_object())
		{
			return nullptr;
		}
		const std::string typeName = componentNode["type"].get<std::string>();
		auto component = GameObjectComponent::ComponentFactory::GetInstance()->Create(typeName, object.get());
		if (!component)
		{
			Logger::Log("プレハブのコンポーネント種類を読み込めません: " + typeName + "\n", Logger::LogLevel::Error);
			return nullptr;
		}
		GameObjectComponent::Component* added = object->AddComponent(std::move(component), typeName);
		if (auto* editable = dynamic_cast<JsonEditableBase*>(added))
		{
			editable->Deserialize(componentNode["fields"]);
		}
		else if (!componentNode["fields"].empty())
		{
			Logger::Log("プレハブのfieldsを読めないコンポーネントです: " + typeName + "\n", Logger::LogLevel::Error);
			return nullptr;
		}
		added->SetEnabled(componentNode["enabled"].get<bool>());
	}

	std::unordered_set<std::string> childNames;
	for (const auto& childNode : node["children"])
	{
		auto child = DeserializePrefabNode(childNode, object3dCommon, lightManager);
		if (!child || !childNames.insert(child->GetName()).second)
		{
			return nullptr;
		}
		const std::string childName = child->GetName();
		object->AddChild(childName, std::move(child));
	}
	return object;
}
}

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
	for (auto* obj : tempObjects)
	{
		if (std::find(gameObjects_.begin(), gameObjects_.end(), obj) != gameObjects_.end() && obj->IsActive() && !obj->IsPendingDestroy())
		{
			obj->LateUpdate();
		}
	}

	// 破棄保留中のオブジェクトを安全に解放
	ClearPendingDestroyObjects();
}

bool GameObjectManager::IsRenderableVisible(const IRenderable3d* renderable)
{
	// フレームが変わったら数を締める（Update を呼ばないシーンもあるので、フレーム番号で見る）
	const uint64_t frame = TimeManager::GetInstance().GetFrameCount();
	if (frame != countedFrame_)
	{
		lastFrameDrawnCount_ = drawnCount_;
		lastFrameCulledCount_ = culledCount_;
		drawnCount_ = 0;
		culledCount_ = 0;
		countedFrame_ = frame;
	}

	// 影を描いている間はライトの行列、そうでなければビューの行列で判定する
	const Matrix4x4* viewProjection = shadowCullingViewProjection_ ? shadowCullingViewProjection_ : viewCullingViewProjection_;
	AABB bounds;
	if (!cullingEnabled_ || !renderable || !viewProjection || !renderable->TryGetLocalBounds(bounds))
	{
		++drawnCount_;
		return true;
	}
	if (IsAabbOutsideClip(bounds, renderable->GetWorldMatrix() * *viewProjection))
	{
		++culledCount_;
		return false;
	}
	++drawnCount_;
	return true;
}

void GameObjectManager::Draw3D(CameraManager* camera)
{
	// このビューのカメラで省く。描き終わったら外す（影や別のビューで古い行列を使わないように）
	Camera* viewCamera = camera ? camera->GetActiveCamera() : nullptr;
	viewCullingViewProjection_ = viewCamera ? &viewCamera->GetViewProjectionMatrix() : nullptr;
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
	viewCullingViewProjection_ = nullptr;
}

void GameObjectManager::DrawTransparent(CameraManager* camera, const std::vector<Object3d*>& sceneObjects)
{
	if (!camera || !camera->GetActiveCamera())
	{
		return;
	}
	const Vector3 cameraPosition = camera->GetActiveCamera()->GetTranslate();
	// GameObject の半透明だけ、このビューのカメラで省く（シーンが直接持つ物は今まで通り全部描く）
	viewCullingViewProjection_ = &camera->GetActiveCamera()->GetViewProjectionMatrix();
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
				renderable->GetRenderQueue() == RenderQueue::Transparent &&
				IsRenderableVisible(renderable))
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
	viewCullingViewProjection_ = nullptr;
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
	// カメラを渡されないときは行列を確定できないので省かない
	Camera* viewCamera = camera ? camera->GetActiveCamera() : nullptr;
	viewCullingViewProjection_ = viewCamera ? &viewCamera->GetViewProjectionMatrix() : nullptr;
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
	viewCullingViewProjection_ = nullptr;
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

std::unique_ptr<GameObject> GameObjectManager::LoadPrefab(const std::string& prefabPath) const
{
	if (prefabPath.empty() || !cachedObject3dCommon_ || !cachedLightManager_)
	{
		Logger::Log("プレハブを読み込むためのパスまたは共有システムがありません。\n", Logger::LogLevel::Error);
		return nullptr;
	}
	const std::filesystem::path fullPath = PathManager::GetApplicationResourceRoot() / "json" / "prefab" /
		std::filesystem::path(prefabPath).filename();
	std::ifstream input(fullPath);
	if (!input)
	{
		Logger::Log("プレハブを開けません: " + fullPath.string() + "\n", Logger::LogLevel::Error);
		return nullptr;
	}
	try
	{
		nlohmann::json json;
		input >> json;
		auto object = DeserializePrefabNode(json, cachedObject3dCommon_, cachedLightManager_);
		if (!object)
		{
			Logger::Log("プレハブの形式が壊れています: " + fullPath.string() + "\n", Logger::LogLevel::Error);
		}
		return object;
	}
	catch (const std::exception& error)
	{
		Logger::Log("プレハブを読み込めません: " + fullPath.string() + " (" + error.what() + ")\n", Logger::LogLevel::Error);
		return nullptr;
	}
}

GameObject* GameObjectManager::Instantiate(const std::string& prefabPath, const Transform& transform)
{
	auto object = LoadPrefab(prefabPath);
	if (!object)
	{
		return nullptr;
	}
	object->SetScale(transform.scale);
	object->SetRotation(transform.rotate);
	object->SetPosition(transform.translate);
	object->UpdateWorldMatrix();
	GameObject* result = object.get();
	dynamicGameObjects_.push_back(std::move(object));
	Register(result);
	return result;
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
