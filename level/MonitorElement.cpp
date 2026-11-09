#include "level/MonitorElement.h"

#include <cmath>
#include <numbers>

#include "base/Camera.h"
#include "base/GraphicsTypes.h"
#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"
#include "graphics/3d/IRenderable3d.h"
#include "graphics/3d/Model.h"
#include "graphics/3d/Object3d.h"
#include "graphics/view/RenderView.h"
#include "graphics/view/StageMonitor.h"
#include "manager/scene/CameraManager.h"
#include "math/Frustum.h"
#include "scene/SceneContext.h"
#include "level/CameraElement.h"
#include "level/Level.h"
#ifdef USE_IMGUI
#include "editor/SceneGizmo.h"
#include "editor/SelectionContext.h"
#include "externals/imgui/imgui.h"
#include "math/MatrixFunc.h"
#endif

namespace KCE
{
namespace
{
constexpr const char* kScreenModel = "plane";
constexpr const char* kScreenTag = "StageMonitor";
constexpr Vector3 kDefaultScreenScale = { 1.2f, 0.675f, 1.0f };
// plane の表は -Z 側（カメラの向こう）を向いているので、回転なしで、カメラ側へ向く
constexpr Vector3 kDefaultScreenRotation = { 0.0f, 0.0f, 0.0f };
constexpr Vector3 kDefaultScreenPosition = { 0.0f, 2.4f, 9.0f };
constexpr float kUvCenter = 0.5f;
constexpr float kDragSpeed = 0.05f;

constexpr MonitorScreenAspect kAspects[] = {
	MonitorScreenAspect::Aspect16x9, MonitorScreenAspect::Aspect4x3, MonitorScreenAspect::Aspect1x1,
	MonitorScreenAspect::Aspect9x16, MonitorScreenAspect::Aspect21x9, MonitorScreenAspect::Free
};
constexpr const char* kAspectLabels[] = { "16:9", "4:3", "1:1", "9:16", "21:9", "自由" };

const char* AspectToString(MonitorScreenAspect aspect)
{
	switch (aspect)
	{
	case MonitorScreenAspect::Aspect16x9: return "16:9";
	case MonitorScreenAspect::Aspect4x3: return "4:3";
	case MonitorScreenAspect::Aspect1x1: return "1:1";
	case MonitorScreenAspect::Aspect9x16: return "9:16";
	case MonitorScreenAspect::Aspect21x9: return "21:9";
	case MonitorScreenAspect::Free: return "Free";
	}
	return "Free";
}

MonitorScreenAspect AspectFromString(const std::string& value)
{
	for (const MonitorScreenAspect aspect : kAspects)
	{
		if (value == AspectToString(aspect)) { return aspect; }
	}
	return MonitorScreenAspect::Free;
}

float AspectRatio(MonitorScreenAspect aspect)
{
	switch (aspect)
	{
	case MonitorScreenAspect::Aspect16x9: return 16.0f / 9.0f;
	case MonitorScreenAspect::Aspect4x3: return 4.0f / 3.0f;
	case MonitorScreenAspect::Aspect1x1: return 1.0f;
	case MonitorScreenAspect::Aspect9x16: return 9.0f / 16.0f;
	case MonitorScreenAspect::Aspect21x9: return 21.0f / 9.0f;
	case MonitorScreenAspect::Free: return 0.0f;
	}
	return 0.0f;
}

/** @brief 固定比率は、幅を基準に高さを決める。縦長へ切り替えても操作の基準を変えない */
void ConstrainScale(Vector3& scale, MonitorScreenAspect aspect)
{
	const float ratio = AspectRatio(aspect);
	if (ratio > 0.0f) { scale.y = scale.x / ratio; }
}

bool ReadFloats(const nlohmann::json& json, size_t count, float* out)
{
	if (!json.is_array() || json.size() != count) { return false; }
	for (size_t i = 0; i < count; ++i)
	{
		if (!json[i].is_number()) { return false; }
		out[i] = json[i].get<float>();
	}
	return true;
}

/** @brief 画面の板を作って登録する。作れなければ nullptr */
GameObject* CreateScreen(const std::string& name, const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
	if (!GameObjectManager::HasInstance()) { return nullptr; }
	GameObject* screen = GameObjectManager::GetInstance()->CreateGameObject(name, kScreenTag);
	if (!screen) { return nullptr; }
	screen->SetModel(kScreenModel);
	screen->SetPosition(position);
	screen->SetRotation(rotation);
	screen->SetScale(scale);
	if (IRenderable3d* renderable = screen->GetRenderable3d())
	{
		// 映像の色をそのまま出す
		renderable->SetRenderingType(RenderingType::Forward);
		renderable->SetEnableLighting(false);
	}
	return screen;
}
} // namespace

/** @brief 作成関数から、private なコンストラクタを呼ぶための入り口 */
struct MonitorElementFactory
{
	static std::unique_ptr<MonitorElement> Make(const std::string& name, const std::string& cameraName, MonitorScreenAspect aspect,
		const Vector3& position, const Vector3& rotation, const Vector3& scale, const SceneContext& context)
	{
		if (name.empty() || !context.cameraManager) { return nullptr; }
		GameObject* screen = CreateScreen(name, position, rotation, scale);
		if (!screen) { return nullptr; }
		return std::unique_ptr<MonitorElement>(new MonitorElement(name, cameraName, aspect, screen, context.cameraManager));
	}
};

namespace
{
std::unique_ptr<ILevelElement> CreateFromJson(const nlohmann::json& entry, const SceneContext& context)
{
	if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string()
		|| !entry.contains("screen") || !entry["screen"].is_object()
		|| !entry.contains("camera") || !entry["camera"].is_string())
	{
		// version 1 の旧形式（カメラをモニター内に持つ）もここに来る
		return nullptr;
	}
	const auto& screen = entry["screen"];
	Vector3 position = kDefaultScreenPosition;
	Vector3 rotation = kDefaultScreenRotation;
	Vector3 scale = kDefaultScreenScale;
	if (screen.contains("position")) { ReadFloats(screen["position"], 3, &position.x); }
	if (screen.contains("rotation")) { ReadFloats(screen["rotation"], 3, &rotation.x); }
	if (screen.contains("scale")) { ReadFloats(screen["scale"], 3, &scale.x); }
	// aspect が無い旧ファイルは、保存済みの縦横を変えない
	const MonitorScreenAspect aspect = entry.contains("aspect") && entry["aspect"].is_string()
		? AspectFromString(entry["aspect"].get<std::string>()) : MonitorScreenAspect::Free;
	return MonitorElementFactory::Make(entry["name"].get<std::string>(), entry["camera"].get<std::string>(), aspect, position, rotation, scale, context);
}

std::unique_ptr<ILevelElement> CreateDefault(const std::string& name, const SceneContext& context)
{
	return MonitorElementFactory::Make(name, std::string(), MonitorScreenAspect::Aspect16x9, kDefaultScreenPosition, kDefaultScreenRotation, kDefaultScreenScale, context);
}
} // namespace

MonitorElement::MonitorElement(std::string name, std::string cameraName, MonitorScreenAspect aspect, GameObject* screen, CameraManager* cameraManager)
	: ILevelElement(std::move(name)), screen_(screen), cameraName_(std::move(cameraName)), aspect_(aspect), cameraManager_(cameraManager)
{
	monitor_ = std::make_unique<StageMonitor>();
	monitor_->SetScreen(screen_);
}

MonitorElement::~MonitorElement()
{
	// 画面のテクスチャを戻してから、板を外す。映像（カメラ側）は、このあとで壊れても構わない順にしてある
	monitor_.reset();
	if (screen_ && GameObjectManager::HasInstance())
	{
		GameObjectManager::GetInstance()->Unregister(screen_);
	}
	screen_ = nullptr;
}

LevelElementType MonitorElement::MakeType()
{
	LevelElementType type;
	type.key = "monitors";
	type.displayName = "モニター";
	type.defaultName = "Monitor";
	type.create = &CreateFromJson;
	type.createDefault = &CreateDefault;
	return type;
}

void MonitorElement::UpdateCrop()
{
	auto* object = screen_ ? dynamic_cast<Object3d*>(screen_->GetRenderable3d()) : nullptr;
	Model* model = object ? object->GetModel() : nullptr;
	if (!model) { return; }
	const CameraElement* camera = GetLevel() ? GetLevel()->Find<CameraElement>(cameraName_) : nullptr;
	const Vector3 scale = screen_->GetScale();
	const float screenAspect = scale.y != 0.0f ? std::abs(scale.x / scale.y) : 0.0f;
	const float cameraAspect = camera && camera->GetHeight() != 0
		? static_cast<float>(camera->GetWidth()) / static_cast<float>(camera->GetHeight()) : 0.0f;
	cropScreenAspect_ = screenAspect;
	cropCameraAspect_ = cameraAspect;
	Vector3 uvScale{ 1.0f, 1.0f, 1.0f };
	Vector3 uvTranslate{};
	if (screenAspect > 0.0f && cameraAspect > 0.0f)
	{
		if (screenAspect < cameraAspect)
		{
			uvScale.x = screenAspect / cameraAspect;
			uvTranslate.x = (1.0f - uvScale.x) * kUvCenter;
		}
		else if (screenAspect > cameraAspect)
		{
			uvScale.y = cameraAspect / screenAspect;
			uvTranslate.y = (1.0f - uvScale.y) * kUvCenter;
		}
	}
	model->SetUVScale(uvScale);
	model->SetUVTranslate(uvTranslate);
}

void MonitorElement::OnLevelBound()
{
	if (screen_)
	{
		screen_->SetName(MakeObjectName(GetName()));
	}
}

bool MonitorElement::OnRename(const std::string& newName)
{
	if (screen_)
	{
		screen_->SetName(MakeObjectName(newName));
	}
	return true;
}

void MonitorElement::OnElementRenamed(const std::string& oldName, const std::string& newName)
{
	// 映すカメラの名前が変わったら、参照を追従させる
	if (cameraName_ == oldName)
	{
		cameraName_ = newName;
	}
}

void MonitorElement::Update(float)
{
	if (!monitor_ || !screen_) { return; }
	CameraElement* camera = GetLevel() ? GetLevel()->Find<CameraElement>(cameraName_) : nullptr;
	RenderView* view = camera ? camera->GetView() : nullptr;
	// カメラが差し替わった（解像度が変わった・消えた）ときだけつなぎ直す
	if (view != attachedView_)
	{
		attachedView_ = view;
		monitor_->AttachView(view);
		cropScreenAspect_ = -1.0f;
	}
	const Vector3 scale = screen_->GetScale();
	const float screenAspect = scale.y != 0.0f ? std::abs(scale.x / scale.y) : 0.0f;
	const float cameraAspect = camera && camera->GetHeight() != 0
		? static_cast<float>(camera->GetWidth()) / static_cast<float>(camera->GetHeight()) : 0.0f;
	if (screenAspect != cropScreenAspect_ || cameraAspect != cropCameraAspect_)
	{
		UpdateCrop();
	}
	// 本編カメラに画面が見えているときだけ、映すカメラを描かせる
	Camera* primary = cameraManager_ ? cameraManager_->GetPrimaryCamera() : nullptr;
	bool visible = primary == nullptr;
	if (primary)
	{
		visible = monitor_->IsScreenVisible(Frustum::FromViewProjection(primary->GetViewProjectionMatrix()));
	}
	if (visible && camera)
	{
		camera->RequestView();
	}
	monitor_->Update();
}

nlohmann::ordered_json MonitorElement::Serialize() const
{
	nlohmann::ordered_json json;
	json["name"] = GetName();
	const Vector3 position = screen_->GetPosition();
	const Vector3 rotation = screen_->GetRotation();
	const Vector3 scale = screen_->GetScale();
	json["screen"] = {
		{ "position", { position.x, position.y, position.z } },
		{ "rotation", { rotation.x, rotation.y, rotation.z } },
		{ "scale", { scale.x, scale.y, scale.z } }
	};
	json["camera"] = cameraName_;
	json["aspect"] = AspectToString(aspect_);
	return json;
}

#ifdef USE_IMGUI
void MonitorElement::DrawHierarchyRow()
{
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	const bool isSelected = primary.kind == SelectionKind::StageMonitor && primary.name == GetName();
	ImGui::PushID(this);
	if (ImGui::Selectable(GetName().c_str(), isSelected))
	{
		SelectionItem item;
		item.kind = SelectionKind::StageMonitor;
		item.name = GetName();
		SelectionContext::GetInstance()->Select(item);
	}
	ImGui::PopID();
}

void MonitorElement::DrawInspector(const SelectionItem& selected)
{
	if (selected.kind != SelectionKind::StageMonitor || selected.name != GetName() || !screen_)
	{
		return;
	}
	ImGui::SeparatorText("画面");
	if (ImGui::BeginCombo("比率", kAspectLabels[static_cast<size_t>(aspect_)]))
	{
		for (size_t i = 0; i < std::size(kAspects); ++i)
		{
			if (ImGui::Selectable(kAspectLabels[i], aspect_ == kAspects[i]))
			{
				aspect_ = kAspects[i];
				Vector3 scale = screen_->GetScale();
				ConstrainScale(scale, aspect_);
				screen_->SetScale(scale);
			}
		}
		ImGui::EndCombo();
	}
	Vector3 position = screen_->GetPosition();
	if (ImGui::DragFloat3("位置", &position.x, kDragSpeed)) { screen_->SetPosition(position); }
	Vector3 rotation = screen_->GetRotation();
	if (ImGui::DragFloat3("回転", &rotation.x, kDragSpeed)) { screen_->SetRotation(rotation); }
	Vector3 scale = screen_->GetScale();
	if (aspect_ == MonitorScreenAspect::Free)
	{
		if (ImGui::DragFloat3("大きさ", &scale.x, kDragSpeed)) { screen_->SetScale(scale); }
	}
	else
	{
		float width = scale.x;
		if (ImGui::DragFloat("幅", &width, kDragSpeed))
		{
			scale.x = width;
			ConstrainScale(scale, aspect_);
			screen_->SetScale(scale);
		}
	}

	ImGui::SeparatorText("映すカメラ");
	if (ImGui::BeginCombo("カメラ", cameraName_.empty() ? "なし" : cameraName_.c_str()))
	{
		if (ImGui::Selectable("なし", cameraName_.empty())) { cameraName_.clear(); }
		if (GetLevel())
		{
			for (const auto& element : GetLevel()->GetElements())
			{
				if (std::string(element->GetTypeKey()) == "cameras" && ImGui::Selectable(element->GetName().c_str(), cameraName_ == element->GetName()))
				{
					cameraName_ = element->GetName();
				}
			}
		}
		ImGui::EndCombo();
	}
	if (!cameraName_.empty() && ImGui::Button("カメラを選択"))
	{
		SelectionItem cameraItem;
		cameraItem.kind = SelectionKind::Camera;
		cameraItem.name = cameraName_;
		SelectionContext::GetInstance()->Select(cameraItem);
	}
}

bool MonitorElement::GetGizmoPose(const SelectionItem& selected, Matrix4x4& world, uint32_t& operations) const
{
	if (selected.kind != SelectionKind::StageMonitor || selected.name != GetName() || !screen_)
	{
		return false;
	}
	world = MakeAffineMatrix(screen_->GetScale(), screen_->GetRotation(), screen_->GetPosition());
	operations = kGizmoAll;
	return true;
}

void MonitorElement::ApplyGizmo(const SelectionItem& selected, const GizmoResult& result, uint32_t)
{
	if (selected.kind != SelectionKind::StageMonitor || selected.name != GetName() || !screen_)
	{
		return;
	}
	Vector3 scale = result.scale;
	ConstrainScale(scale, aspect_);
	screen_->SetPosition(result.translate);
	screen_->SetRotation(result.rotate.ToEuler());
	screen_->SetScale(scale);
}
#endif
} // namespace KCE
