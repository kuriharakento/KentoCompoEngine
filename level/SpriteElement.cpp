#include "level/SpriteElement.h"

#include <algorithm>
#include <cmath>

#include "graphics/2d/SpriteCommon.h"
#include "graphics/2d/SpriteLayer.h"
#include "manager/graphics/TextureManager.h"
#include "scene/SceneContext.h"
#ifdef USE_IMGUI
#include "editor/SelectionContext.h"
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
namespace
{
// 新しく置くスプライトの画像。エンジンが必ず読み込んでいる 1x1 の白
constexpr const char* kDefaultTexture = "textures/white1x1.png";
constexpr float kDefaultSize = 256.0f;
constexpr float kRadToDeg = 57.29578f;

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

std::unique_ptr<Sprite> MakeSprite(const SceneContext& context, const std::string& texturePath)
{
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(context.spriteCommon, texturePath);
	return sprite;
}

std::unique_ptr<ILevelElement> CreateFromJson(const nlohmann::json& entry, const SceneContext& context)
{
	if (!context.spriteCommon || !context.sprites || !entry.is_object() || !entry.contains("name") || !entry["name"].is_string())
	{
		return nullptr;
	}
	const std::string name = entry["name"].get<std::string>();
	if (name.empty() || context.sprites->Find(name))
	{
		return nullptr;
	}
	const std::string texture = (entry.contains("texture") && entry["texture"].is_string()) ? entry["texture"].get<std::string>() : std::string(kDefaultTexture);
	auto sprite = MakeSprite(context, texture);

	float values[4] = {};
	// 画像の大きさで始まっているので、書いてあるときだけ上書きする
	if (entry.contains("size") && ReadFloats(entry["size"], 2, values)) { sprite->SetSize({ values[0], values[1] }); }
	if (entry.contains("position") && ReadFloats(entry["position"], 2, values)) { sprite->SetPosition({ values[0], values[1] }); }
	if (entry.contains("anchor") && ReadFloats(entry["anchor"], 2, values)) { sprite->SetAnchorPoint({ values[0], values[1] }); }
	if (entry.contains("color") && ReadFloats(entry["color"], 4, values)) { sprite->SetColor({ values[0], values[1], values[2], values[3] }); }
	if (entry.contains("rotation") && entry["rotation"].is_number()) { sprite->SetRotation(entry["rotation"].get<float>()); }
	const bool flipX = entry.contains("flipX") && entry["flipX"].is_boolean() && entry["flipX"].get<bool>();
	sprite->SetFlipX(flipX);
	const int order = (entry.contains("order") && entry["order"].is_number_integer()) ? entry["order"].get<int>() : 0;
	return std::make_unique<SpriteElement>(name, texture, std::move(sprite), context.sprites, order);
}

std::unique_ptr<ILevelElement> CreateDefault(const std::string& name, const SceneContext& context)
{
	if (!context.spriteCommon || !context.sprites || context.sprites->Find(name))
	{
		return nullptr;
	}
	auto sprite = MakeSprite(context, kDefaultTexture);
	// 画面の中央に、見える大きさで出す
	sprite->SetSize({ kDefaultSize, kDefaultSize });
	sprite->SetPosition({ Sprite::kCoordinateWidth * 0.5f, Sprite::kCoordinateHeight * 0.5f });
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	return std::make_unique<SpriteElement>(name, kDefaultTexture, std::move(sprite), context.sprites, 0);
}
} // namespace

SpriteElement::SpriteElement(std::string name, std::string texturePath, std::unique_ptr<Sprite> sprite, SpriteLayer* layer, int order)
	: ILevelElement(std::move(name)), texturePath_(std::move(texturePath)), sprite_(std::move(sprite)), layer_(layer), order_(order)
{
	if (layer_ && sprite_)
	{
		layer_->Register(GetName(), sprite_.get(), order_);
	}
}

SpriteElement::~SpriteElement()
{
	// 所有物を壊す前に、描画側の登録を外す
	if (layer_ && sprite_)
	{
		layer_->Unregister(sprite_.get());
	}
}

LevelElementType SpriteElement::MakeType()
{
	LevelElementType type;
	type.key = "sprites";
	type.displayName = "スプライト";
	type.defaultName = "Sprite";
	type.create = &CreateFromJson;
	type.createDefault = &CreateDefault;
	return type;
}

bool SpriteElement::OnRename(const std::string& newName)
{
	// 登録名は SpriteLayer が持っている。ほかのスプライトと名前がぶつかるなら変えない
	if (layer_ && sprite_)
	{
		return layer_->Rename(sprite_.get(), newName);
	}
	return true;
}

nlohmann::ordered_json SpriteElement::Serialize() const
{
	nlohmann::ordered_json json;
	json["name"] = GetName();
	json["texture"] = texturePath_;
	json["position"] = { sprite_->GetPosition().x, sprite_->GetPosition().y };
	json["size"] = { sprite_->GetSize().x, sprite_->GetSize().y };
	json["rotation"] = sprite_->GetRotation();
	json["anchor"] = { sprite_->GetAnchorPoint().x, sprite_->GetAnchorPoint().y };
	const Vector4& color = sprite_->GetColor();
	json["color"] = { color.x, color.y, color.z, color.w };
	json["flipX"] = sprite_->GetFlipX();
	json["order"] = order_;
	return json;
}

#ifdef USE_IMGUI
void SpriteElement::DrawHierarchyRow()
{
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	const bool isSelected = primary.kind == SelectionKind::Sprite2D && primary.name == GetName();
	ImGui::PushID(this);
	if (ImGui::Selectable(GetName().c_str(), isSelected))
	{
		SelectionItem item;
		item.kind = SelectionKind::Sprite2D;
		item.name = GetName();
		SelectionContext::GetInstance()->Select(item);
	}
	ImGui::PopID();
}

void SpriteElement::DrawInspector(const SelectionItem& selected)
{
	if (selected.kind != SelectionKind::Sprite2D || selected.name != GetName() || !sprite_)
	{
		return;
	}
	ImGui::PushID(this);
	ImGui::SeparatorText(GetName().c_str());

	// 画像。読み込み済みの 2D 画像から選ぶ。カーソルを置くとプレビューが出る
	const auto textures = TextureManager::GetInstance()->GetLoadedTextures();
	if (ImGui::BeginCombo("画像", texturePath_.c_str()))
	{
		for (const auto& texture : textures)
		{
			if (!texture.previewable) { continue; }
			if (ImGui::Selectable(texture.path.c_str(), texture.path == texturePath_))
			{
				texturePath_ = texture.path;
				// 画像を替えると大きさが画像に合うので、替える前の大きさを戻す
				const Vector2 size = sprite_->GetSize();
				sprite_->SetTexture(texturePath_);
				sprite_->SetSize(size);
			}
			if (ImGui::IsItemHovered() && texture.gpuHandle != 0 && texture.width > 0 && texture.height > 0)
			{
				const float scale = (std::min)(192.0f / static_cast<float>(texture.width), 192.0f / static_cast<float>(texture.height));
				ImGui::BeginTooltip();
				ImGui::Image(static_cast<ImTextureID>(texture.gpuHandle), ImVec2(static_cast<float>(texture.width) * scale, static_cast<float>(texture.height) * scale));
				ImGui::EndTooltip();
			}
		}
		ImGui::EndCombo();
	}
	const auto current = std::find_if(textures.begin(), textures.end(), [this](const auto& t) { return t.path == texturePath_; });
	if (current != textures.end() && current->previewable && current->gpuHandle != 0 && current->width > 0 && current->height > 0)
	{
		const float scale = (std::min)(96.0f / static_cast<float>(current->width), 96.0f / static_cast<float>(current->height));
		ImGui::Image(static_cast<ImTextureID>(current->gpuHandle), ImVec2(static_cast<float>(current->width) * scale, static_cast<float>(current->height) * scale));
	}

	Vector2 position = sprite_->GetPosition();
	if (ImGui::DragFloat2("位置", &position.x, 1.0f)) { sprite_->SetPosition(position); }
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("1920x1080 の画面での位置（アンカーの点）"); }
	Vector2 size = sprite_->GetSize();
	if (ImGui::DragFloat2("大きさ", &size.x, 1.0f, 1.0f, 8192.0f)) { sprite_->SetSize(size); }
	float rotationDeg = sprite_->GetRotation() * kRadToDeg;
	if (ImGui::DragFloat("回転(度)", &rotationDeg, 0.5f)) { sprite_->SetRotation(rotationDeg / kRadToDeg); }
	Vector2 anchor = sprite_->GetAnchorPoint();
	if (ImGui::DragFloat2("アンカー", &anchor.x, 0.01f, 0.0f, 1.0f)) { sprite_->SetAnchorPoint(anchor); }
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("位置の基準になる点。0,0 が左上、0.5,0.5 が中心"); }
	Vector4 color = sprite_->GetColor();
	if (ImGui::ColorEdit4("色", &color.x)) { sprite_->SetColor(color); }
	bool flipX = sprite_->GetFlipX();
	if (ImGui::Checkbox("左右反転", &flipX)) { sprite_->SetFlipX(flipX); }
	int order = order_;
	if (ImGui::DragInt("描く順", &order, 0.2f))
	{
		order_ = order;
		if (layer_) { layer_->SetOrder(sprite_.get(), order_); }
	}
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("小さいほど奥に描く"); }
	ImGui::PopID();
}
#endif
} // namespace KCE
