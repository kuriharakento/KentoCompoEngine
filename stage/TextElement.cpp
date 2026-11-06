#include "stage/TextElement.h"

#include "graphics/text/Text3DRenderer.h"
#include "scene/SceneContext.h"
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
constexpr const char* kDefaultText = "テキスト";
// 新しく置く文字を、カメラの前に見える位置へ出す
constexpr float kDefaultZ = 5.0f;

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

std::unique_ptr<IStageElement> CreateFromJson(const nlohmann::json& entry, const SceneContext& context)
{
	if (!context.text3D || !entry.is_object() || !entry.contains("name") || !entry["name"].is_string()
		|| !entry.contains("text") || !entry["text"].is_string())
	{
		return nullptr;
	}
	const std::string name = entry["name"].get<std::string>();
	if (name.empty() || context.text3D->Find(name))
	{
		return nullptr;
	}
	auto mesh = std::make_unique<TextMesh3D>();
	mesh->SetText(entry["text"].get<std::string>());
	TextMesh3D::Params& params = mesh->GetParams();
	if (entry.contains("position")) { ReadFloats(entry["position"], 3, &params.position.x); }
	if (entry.contains("rotation")) { ReadFloats(entry["rotation"], 4, &params.rotation.x); }
	if (entry.contains("scale") && entry["scale"].is_number()) { params.scale = entry["scale"].get<float>(); }
	if (entry.contains("size") && entry["size"].is_number()) { params.size = entry["size"].get<float>(); }
	if (entry.contains("color")) { ReadFloats(entry["color"], 4, &params.color.x); }
	if (entry.contains("style") && entry["style"].is_string()) { params.style = TextAppearStyleFromString(entry["style"].get<std::string>()); }
	params.reveal = TextMesh3D::kRevealAll;
	params.exit = 0.0f;
	return std::make_unique<TextElement>(name, std::move(mesh), context.text3D);
}

std::unique_ptr<IStageElement> CreateDefault(const std::string& name, const SceneContext& context)
{
	if (!context.text3D || context.text3D->Find(name))
	{
		return nullptr;
	}
	auto mesh = std::make_unique<TextMesh3D>();
	mesh->SetText(kDefaultText);
	mesh->GetParams().position = { 0.0f, 1.0f, kDefaultZ };
	mesh->GetParams().reveal = TextMesh3D::kRevealAll;
	return std::make_unique<TextElement>(name, std::move(mesh), context.text3D);
}
} // namespace

TextElement::TextElement(std::string name, std::unique_ptr<TextMesh3D> mesh, Text3DRenderer* renderer)
	: IStageElement(std::move(name)), mesh_(std::move(mesh)), renderer_(renderer)
{
	if (renderer_ && mesh_)
	{
		renderer_->Register(GetName(), mesh_.get());
	}
}

TextElement::~TextElement()
{
	// 所有物を壊す前に、描画側の登録を外す
	if (renderer_ && mesh_)
	{
		renderer_->Unregister(mesh_.get());
	}
}

StageElementType TextElement::MakeType()
{
	StageElementType type;
	type.key = "texts3d";
	type.displayName = "3D文字";
	type.defaultName = "Text";
	type.create = &CreateFromJson;
	type.createDefault = &CreateDefault;
	return type;
}

bool TextElement::OnRename(const std::string& newName)
{
	// 登録名は Text3DRenderer が持っている。ほかの文字と名前がぶつかるなら変えない
	if (renderer_ && mesh_)
	{
		if (renderer_->Find(newName))
		{
			return false;
		}
		renderer_->Unregister(mesh_.get());
		renderer_->Register(newName, mesh_.get());
	}
	return true;
}

nlohmann::ordered_json TextElement::Serialize() const
{
	nlohmann::ordered_json json;
	json["name"] = GetName();
	json["text"] = mesh_->GetText();
	const TextMesh3D::Params& params = mesh_->GetParams();
	json["position"] = { params.position.x, params.position.y, params.position.z };
	json["rotation"] = { params.rotation.x, params.rotation.y, params.rotation.z, params.rotation.w };
	json["scale"] = params.scale;
	json["size"] = params.size;
	json["color"] = { params.color.x, params.color.y, params.color.z, params.color.w };
	json["style"] = TextAppearStyleToString(params.style);
	return json;
}

#ifdef USE_IMGUI
void TextElement::DrawHierarchyRow()
{
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	const bool isSelected = primary.kind == SelectionKind::Text3D && primary.name == GetName();
	ImGui::PushID(this);
	if (ImGui::Selectable(GetName().c_str(), isSelected))
	{
		SelectionItem item;
		item.kind = SelectionKind::Text3D;
		item.name = GetName();
		SelectionContext::GetInstance()->Select(item);
	}
	ImGui::PopID();
}

void TextElement::DrawInspector(const SelectionItem&)
{
	// 中身は Text3DRenderer の Inspector が描く
}

bool TextElement::GetGizmoPose(const SelectionItem& selected, Matrix4x4& world, uint32_t& operations) const
{
	if (selected.kind != SelectionKind::Text3D || selected.name != GetName())
	{
		return false;
	}
	const TextMesh3D::Params& params = mesh_->GetParams();
	world = MakeAffineMatrix({ params.scale, params.scale, params.scale }, params.rotation.ToEuler(), params.position);
	operations = kGizmoAll;
	return true;
}

void TextElement::ApplyGizmo(const SelectionItem& selected, const GizmoResult& result, uint32_t)
{
	if (selected.kind != SelectionKind::Text3D || selected.name != GetName())
	{
		return;
	}
	TextMesh3D::Params& params = mesh_->GetParams();
	params.position = result.translate;
	params.rotation = result.rotate;
	// 文字の拡大率は 1 つだけなので、3 軸の平均にする
	params.scale = (result.scale.x + result.scale.y + result.scale.z) / 3.0f;
}
#endif
} // namespace KCE
