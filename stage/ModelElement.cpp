#include "stage/ModelElement.h"

#include "base/GraphicsTypes.h"
#include "base/Logger.h"
#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"
#include "graphics/3d/IRenderable3d.h"
#include "scene/SceneContext.h"
#ifdef USE_IMGUI
#include "editor/SelectionContext.h"
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
namespace
{
constexpr const char* kStageModelTag = "StageModel";

constexpr const char* kRenderingTypeKey = "renderingType";
constexpr const char* kRenderingForward = "Forward";
constexpr const char* kRenderingDeferred = "Deferred";

void ApplyRenderingType(GameObject& object, RenderingType type)
{
	if (IRenderable3d* renderable = object.GetRenderable3d())
	{
		renderable->SetRenderingType(type);
	}
}

/**
 * @brief 保存された描画方式を読む。無いか読めなければ Forward
 * @details プレハブは描画方式を保存しないので、ステージの要素に別に書く。
 *          新しく置く物と、書かれていない古いファイルは、ライティングを使わない Forward から始める
 */
RenderingType ParseRenderingType(const nlohmann::json& entry)
{
	if (entry.contains(kRenderingTypeKey) && entry[kRenderingTypeKey].is_string() &&
		entry[kRenderingTypeKey].get_ref<const std::string&>() == kRenderingDeferred)
	{
		return RenderingType::Deferred;
	}
	return RenderingType::Forward;
}

/** @brief JSON の1件から要素を作る。形が壊れていれば nullptr */
std::unique_ptr<IStageElement> CreateFromJson(const nlohmann::json& entry, const SceneContext&)
{
	if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string() ||
		!entry.contains("object") || !GameObjectManager::HasInstance())
	{
		return nullptr;
	}
	const std::string name = entry["name"].get<std::string>();
	if (name.empty())
	{
		return nullptr;
	}
	auto object = GameObjectManager::GetInstance()->CreateFromPrefabJson(entry["object"]);
	if (!object)
	{
		return nullptr;
	}
	object->SetName(name);
	ApplyRenderingType(*object, ParseRenderingType(entry));
	GameObject* adopted = GameObjectManager::GetInstance()->Adopt(std::move(object));
	const std::string prefab = (entry.contains("prefab") && entry["prefab"].is_string()) ? entry["prefab"].get<std::string>() : std::string();
	return std::make_unique<ModelElement>(name, prefab, adopted);
}

/** @brief エディタで新規に作る、何も持たない GameObject の要素 */
std::unique_ptr<IStageElement> CreateDefault(const std::string& name, const SceneContext&)
{
	if (!GameObjectManager::HasInstance())
	{
		return nullptr;
	}
	GameObject* object = GameObjectManager::GetInstance()->CreateGameObject(name, kStageModelTag);
	if (!object)
	{
		return nullptr;
	}
	ApplyRenderingType(*object, RenderingType::Forward);
	return std::make_unique<ModelElement>(name, std::string(), object);
}
} // namespace

ModelElement::ModelElement(std::string name, std::string prefabName, GameObject* object)
	: IStageElement(std::move(name)), object_(object), prefabName_(std::move(prefabName))
{
	if (object_)
	{
		object_->SetName(GetName());
	}
	CapturePose();
}

ModelElement::~ModelElement()
{
	// 破棄は GameObjectManager が次のフレームの頭に行う
	if (object_ && GameObjectManager::HasInstance())
	{
		GameObjectManager::GetInstance()->Unregister(object_);
	}
	object_ = nullptr;
}

StageElementType ModelElement::MakeType()
{
	StageElementType type;
	type.key = "models";
	type.displayName = "モデル";
	type.create = &CreateFromJson;
	type.createDefault = &CreateDefault;
	return type;
}

std::unique_ptr<ModelElement> ModelElement::CreateFromPrefab(const std::string& name, const std::string& prefabFile)
{
	if (name.empty() || !GameObjectManager::HasInstance())
	{
		return nullptr;
	}
	auto object = GameObjectManager::GetInstance()->LoadPrefab(prefabFile);
	if (!object)
	{
		return nullptr;
	}
	object->SetName(name);
	// 新しく置く物は Forward から始める。Deferred にしたければインスペクタで切り替える
	ApplyRenderingType(*object, RenderingType::Forward);
	GameObject* adopted = GameObjectManager::GetInstance()->Adopt(std::move(object));
	return std::make_unique<ModelElement>(name, prefabFile, adopted);
}

void ModelElement::CapturePose()
{
	if (!object_)
	{
		return;
	}
	startPose_.position = object_->GetPosition();
	startPose_.rotation = object_->GetRotation();
	startPose_.scale = object_->GetScale();
}

void ModelElement::RestorePose()
{
	if (!object_)
	{
		return;
	}
	object_->SetPosition(startPose_.position);
	object_->SetRotation(startPose_.rotation);
	object_->SetScale(startPose_.scale);
}

void ModelElement::SetPlaying(bool playing)
{
	if (!object_ || playing == playing_)
	{
		return;
	}
	playing_ = playing;
	if (playing)
	{
		// 編集中に動かした置き場所から再生を始める
		CapturePose();
		object_->SetUpdateEnabled(true);
	}
	else
	{
		object_->SetUpdateEnabled(false);
		RestorePose();
	}
}

nlohmann::ordered_json ModelElement::Serialize() const
{
	nlohmann::ordered_json json;
	json["name"] = GetName();
	json["prefab"] = prefabName_;
	if (!object_)
	{
		return json;
	}
	// プレハブの形には描画方式が無いので、ここに別に書く。インスペクタで切り替えた結果が、読み込み直しても残る
	const IRenderable3d* renderable = object_->GetRenderable3d();
	json[kRenderingTypeKey] = (renderable && renderable->GetRenderingType() == RenderingType::Deferred) ? kRenderingDeferred : kRenderingForward;
	nlohmann::json object = object_->SerializePrefab();
	if (playing_)
	{
		// 再生中は動いた先ではなく、再生を始めた時点の置き場所を書く
		Transform transform;
		transform.scale = startPose_.scale;
		transform.rotate = startPose_.rotation;
		transform.translate = startPose_.position;
		object["transform"] = nlohmann::json(transform);
	}
	json["object"] = nlohmann::ordered_json(object);
	return json;
}

#ifdef USE_IMGUI
void ModelElement::DrawHierarchyRow()
{
	if (!object_)
	{
		return;
	}
	ImGui::PushID(this);
	const bool isSelected = SelectionContext::GetInstance()->GetPrimaryGameObject() == object_;
	if (ImGui::Selectable(GetName().c_str(), isSelected))
	{
		SelectionContext::GetInstance()->SelectGameObject(object_);
	}
	ImGui::PopID();
}

void ModelElement::DrawInspector(const SelectionItem&)
{
	// 中身は GameObject の Inspector（GameObjectEditor）が描くので、ここでは足さない
}
#endif
} // namespace KCE
