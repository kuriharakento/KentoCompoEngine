#include "stage/CameraElement.h"

#include <algorithm>

#include "base/Camera.h"
#include "base/Logger.h"
#include "graphics/view/ISubViewProvider.h"
#include "graphics/view/RenderLayer.h"
#include "graphics/view/RenderView.h"
#include "graphics/view/StageMonitor.h"
#include "manager/scene/CameraManager.h"
#include "scene/SceneContext.h"
#ifdef USE_IMGUI
#include "editor/SelectionContext.h"
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
namespace
{
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

CameraElement::State Clamped(CameraElement::State state)
{
	state.width = (std::clamp)(state.width, CameraElement::kMinResolution, CameraElement::kMaxResolution);
	state.height = (std::clamp)(state.height, CameraElement::kMinResolution, CameraElement::kMaxResolution);
	state.framesPerSecond = (std::clamp)(state.framesPerSecond, CameraElement::kMinFramesPerSecond, CameraElement::kMaxFramesPerSecond);
	return state;
}

std::unique_ptr<IStageElement> CreateFromJson(const nlohmann::json& entry, const SceneContext& context)
{
	if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string())
	{
		return nullptr;
	}
	CameraElement::State state;
	if (entry.contains("position")) { ReadFloats(entry["position"], 3, &state.position.x); }
	if (entry.contains("rotation")) { ReadFloats(entry["rotation"], 3, &state.rotation.x); }
	// 数字でない値を get すると例外で落ちるので、型を確かめてから読む
	if (entry.contains("resolution") && entry["resolution"].is_array() && entry["resolution"].size() == 2
		&& entry["resolution"][0].is_number_unsigned() && entry["resolution"][1].is_number_unsigned())
	{
		state.width = entry["resolution"][0].get<uint32_t>();
		state.height = entry["resolution"][1].get<uint32_t>();
	}
	if (entry.contains("framesPerSecond") && entry["framesPerSecond"].is_number())
	{
		state.framesPerSecond = entry["framesPerSecond"].get<float>();
	}
	return CameraElement::Create(entry["name"].get<std::string>(), state, context.cameraManager, context.subViewProvider);
}

std::unique_ptr<IStageElement> CreateDefault(const std::string& name, const SceneContext& context)
{
	CameraElement::State state;
	// 新しく置くカメラは、今映っているカメラの少し前から始める
	state.position = { 0.0f, 1.5f, -5.0f };
	return CameraElement::Create(name, state, context.cameraManager, context.subViewProvider);
}
} // namespace

CameraElement::CameraElement(std::string name, const State& state, CameraManager* cameraManager, ISubViewProvider* provider)
	: IStageElement(std::move(name)), state_(Clamped(state)), cameraManager_(cameraManager), provider_(provider)
{
}

CameraElement::~CameraElement()
{
	// ビューを先に畳んでから、カメラを外す。ビューの破棄は provider が次のフレームの頭まで遅らせる
	if (view_ && provider_)
	{
		provider_->DestroySubView(view_);
	}
	view_ = nullptr;
	if (cameraManager_)
	{
		cameraManager_->RemoveCamera(GetName());
	}
}

StageElementType CameraElement::MakeType()
{
	StageElementType type;
	type.key = "cameras";
	type.displayName = "カメラ";
	type.create = &CreateFromJson;
	type.createDefault = &CreateDefault;
	return type;
}

std::unique_ptr<CameraElement> CameraElement::Create(const std::string& name, const State& state, CameraManager* cameraManager, ISubViewProvider* provider)
{
	if (name.empty() || !cameraManager || !provider || cameraManager->GetCamera(name))
	{
		return nullptr;
	}
	std::unique_ptr<CameraElement> element(new CameraElement(name, state, cameraManager, provider));
	if (!element->Initialize())
	{
		Logger::Log("CameraElement: ステージカメラを作れませんでした: " + name + "\n", Logger::LogLevel::Warning);
		return nullptr;
	}
	return element;
}

bool CameraElement::Initialize()
{
	cameraManager_->AddCamera(GetName());
	Camera* camera = cameraManager_->GetCamera(GetName());
	if (!camera)
	{
		return false;
	}
	camera->SetTranslate(state_.position);
	camera->SetRotate(state_.rotation);
	camera->SetAspectRatio(static_cast<float>(state_.width) / static_cast<float>(state_.height));
	view_ = provider_->CreateSubView(GetName(), state_.width, state_.height);
	if (!view_)
	{
		cameraManager_->RemoveCamera(GetName());
		return false;
	}
	view_->SetCamera(camera);
	// 画面（モニター）自身は映さない。線は省く
	view_->SetLayerMask(kRenderLayerAll & ~StageMonitor::kScreenLayer);
	view_->SetPassEnabled(RenderViewPass::DebugLines, false);
	view_->SetUpdateInterval(1.0f / state_.framesPerSecond);
	view_->SetEnabled(false);
	viewWidth_ = state_.width;
	viewHeight_ = state_.height;
	return true;
}

bool CameraElement::Apply()
{
	Camera* camera = cameraManager_->GetCamera(GetName());
	if (!camera || !view_)
	{
		return false;
	}
	state_ = Clamped(state_);
	const State& applied = state_;
	if (applied.width != viewWidth_ || applied.height != viewHeight_)
	{
		// 解像度が変わるときは、ビューを作り直す。古いほうの破棄は provider が次のフレームの頭まで遅らせる
		RenderView* replacement = provider_->CreateSubView(GetName(), applied.width, applied.height);
		if (!replacement)
		{
			return false;
		}
		const bool wasEnabled = view_->IsEnabled();
		provider_->DestroySubView(view_);
		view_ = replacement;
		view_->SetCamera(camera);
		view_->SetLayerMask(kRenderLayerAll & ~StageMonitor::kScreenLayer);
		view_->SetPassEnabled(RenderViewPass::DebugLines, false);
		view_->SetEnabled(wasEnabled);
		viewWidth_ = applied.width;
		viewHeight_ = applied.height;
	}
	camera->SetAspectRatio(static_cast<float>(applied.width) / static_cast<float>(applied.height));
	view_->SetUpdateInterval(1.0f / applied.framesPerSecond);
	return true;
}

void CameraElement::SetViewEnabled(bool enabled)
{
	if (!view_ || view_->IsEnabled() == enabled)
	{
		return;
	}
	view_->SetEnabled(enabled);
	if (enabled)
	{
		view_->RequestImmediateUpdate();
	}
}

nlohmann::ordered_json CameraElement::Serialize() const
{
	State state = state_;
	// 位置と向きはカメラが持っている（ギズモなどで動かした結果）
	if (const Camera* camera = cameraManager_->GetCamera(GetName()))
	{
		state.position = camera->GetTranslate();
		state.rotation = camera->GetRotateQuaternion().ToEuler();
	}
	nlohmann::ordered_json json;
	json["name"] = GetName();
	json["position"] = { state.position.x, state.position.y, state.position.z };
	json["rotation"] = { state.rotation.x, state.rotation.y, state.rotation.z };
	json["resolution"] = { state.width, state.height };
	json["framesPerSecond"] = state.framesPerSecond;
	return json;
}

#ifdef USE_IMGUI
void CameraElement::DrawHierarchyRow()
{
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	const bool isSelected = primary.kind == SelectionKind::Camera && primary.name == GetName();
	ImGui::PushID(this);
	if (ImGui::Selectable(GetName().c_str(), isSelected))
	{
		SelectionItem item;
		item.kind = SelectionKind::Camera;
		item.name = GetName();
		SelectionContext::GetInstance()->Select(item);
	}
	ImGui::PopID();
}

void CameraElement::DrawInspector(const SelectionItem& selected)
{
	if (selected.kind != SelectionKind::Camera || selected.name != GetName())
	{
		return;
	}
	ImGui::SeparatorText("ステージカメラ");
	int resolution[2] = { static_cast<int>(state_.width), static_cast<int>(state_.height) };
	if (ImGui::DragInt2("解像度", resolution, 1.0f, static_cast<int>(kMinResolution), static_cast<int>(kMaxResolution)))
	{
		state_.width = static_cast<uint32_t>(resolution[0]);
		state_.height = static_cast<uint32_t>(resolution[1]);
	}
	// ドラッグ中に毎フレームビューを作り直さないよう、離したときに反映する
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		Apply();
	}
	if (ImGui::DragFloat("描き直す回数", &state_.framesPerSecond, 1.0f, kMinFramesPerSecond, kMaxFramesPerSecond, "%.0f fps"))
	{
		// 軽いので、ドラッグ中も反映する
		state_.framesPerSecond = (std::clamp)(state_.framesPerSecond, kMinFramesPerSecond, kMaxFramesPerSecond);
		if (view_) { view_->SetUpdateInterval(1.0f / state_.framesPerSecond); }
	}
}
#endif
} // namespace KCE
