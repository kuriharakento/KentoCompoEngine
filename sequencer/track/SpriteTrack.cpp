#include "sequencer/track/SpriteTrack.h"

#include <cstdio>

#include "graphics/2d/Sprite.h"
#include "graphics/2d/SpriteLayer.h"
#include "sequencer/core/BindingContext.h"
#include "sequencer/core/CurveSerialization.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
namespace
{
constexpr size_t kNameBufferSize = 128;
} // namespace

SpriteTrack::SpriteTrack()
{
	SetName("Sprite Track");
	// 対象は名前で探すので役は使わない
	SetBindingRole("");
}

ICurveChannel* SpriteTrack::GetChannel(size_t index)
{
	switch (index)
	{
	case 0: return &positionChannel_;
	case 1: return &sizeChannel_;
	case 2: return &rotationChannel_;
	case 3: return &colorChannel_;
	default: return nullptr;
	}
}

Sprite* SpriteTrack::ResolveSprite(const BindingContext& ctx) const
{
	const SpriteLayer* layer = ctx.GetSpriteLayer();
	return layer ? layer->Find(targetName_) : nullptr;
}

void SpriteTrack::Evaluate(float time, const BindingContext& ctx)
{
	Sprite* sprite = ResolveSprite(ctx);
	if (!sprite)
	{
		return;
	}
	// キーを持たないチャンネルには触れない
	if (!positionCurve_.IsEmpty())
	{
		const Vector3 position = positionCurve_.Evaluate(time);
		sprite->SetPosition({ position.x, position.y });
	}
	if (!sizeCurve_.IsEmpty())
	{
		const Vector3 size = sizeCurve_.Evaluate(time);
		sprite->SetSize({ size.x, size.y });
	}
	if (!rotationCurve_.IsEmpty()) { sprite->SetRotation(rotationCurve_.Evaluate(time)); }
	if (!colorCurve_.IsEmpty()) { sprite->SetColor(colorCurve_.Evaluate(time)); }
}

void SpriteTrack::CaptureState(const BindingContext& ctx)
{
	hasCapturedState_ = false;
	const Sprite* sprite = ResolveSprite(ctx);
	if (!sprite)
	{
		return;
	}
	capturedPosition_ = { sprite->GetPosition().x, sprite->GetPosition().y, 0.0f };
	capturedSize_ = { sprite->GetSize().x, sprite->GetSize().y, 0.0f };
	capturedRotation_ = sprite->GetRotation();
	capturedColor_ = sprite->GetColor();
	hasCapturedState_ = true;
}

void SpriteTrack::RestoreState(const BindingContext& ctx)
{
	Sprite* sprite = ResolveSprite(ctx);
	if (!hasCapturedState_ || !sprite)
	{
		return;
	}
	sprite->SetPosition({ capturedPosition_.x, capturedPosition_.y });
	sprite->SetSize({ capturedSize_.x, capturedSize_.y });
	sprite->SetRotation(capturedRotation_);
	sprite->SetColor(capturedColor_);
}

bool SpriteTrack::RecordKey(float time, const BindingContext& ctx)
{
	const Sprite* sprite = ResolveSprite(ctx);
	if (!sprite)
	{
		return false;
	}
	positionChannel_.SetKey(time, { sprite->GetPosition().x, sprite->GetPosition().y, 0.0f });
	sizeChannel_.SetKey(time, { sprite->GetSize().x, sprite->GetSize().y, 0.0f });
	rotationChannel_.SetKey(time, sprite->GetRotation());
	colorChannel_.SetKey(time, sprite->GetColor());
	return true;
}

void SpriteTrack::UpdateKeyDefaults(const BindingContext& ctx)
{
	const Sprite* sprite = ResolveSprite(ctx);
	if (!sprite)
	{
		return;
	}
	if (positionCurve_.IsEmpty()) { positionChannel_.SetDefaultValue({ sprite->GetPosition().x, sprite->GetPosition().y, 0.0f }); }
	if (sizeCurve_.IsEmpty()) { sizeChannel_.SetDefaultValue({ sprite->GetSize().x, sprite->GetSize().y, 0.0f }); }
	if (rotationCurve_.IsEmpty()) { rotationChannel_.SetDefaultValue(sprite->GetRotation()); }
	if (colorCurve_.IsEmpty()) { colorChannel_.SetDefaultValue(sprite->GetColor()); }
}

nlohmann::json SpriteTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["target"] = targetName_;
	json["position"] = SerializeCurve(positionCurve_);
	json["size"] = SerializeCurve(sizeCurve_);
	json["rotation"] = SerializeCurve(rotationCurve_);
	json["color"] = SerializeCurve(colorCurve_);
	return json;
}

bool SpriteTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}
	DeserializeCommon(json);
	if (json.contains("target") && json["target"].is_string()) { targetName_ = json["target"].get<std::string>(); }
	if (json.contains("position")) { DeserializeCurve(json["position"], positionCurve_); }
	if (json.contains("size")) { DeserializeCurve(json["size"], sizeCurve_); }
	if (json.contains("rotation")) { DeserializeCurve(json["rotation"], rotationCurve_); }
	if (json.contains("color")) { DeserializeCurve(json["color"], colorCurve_); }
	return true;
}

#ifdef USE_IMGUI
bool SpriteTrack::DrawInspector()
{
	BindingContext emptyContext;
	return DrawInspector(emptyContext);
}

bool SpriteTrack::DrawInspector(const BindingContext& ctx)
{
	bool changed = false;

	char nameBuffer[kNameBufferSize];
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", targetName_.c_str());
	if (ImGui::InputText("対象", nameBuffer, sizeof(nameBuffer)))
	{
		targetName_ = nameBuffer;
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Level に置いたスプライトの名前");
	}

	const SpriteLayer* layer = ctx.GetSpriteLayer();
	if (layer && ImGui::BeginCombo("登録済み", targetName_.empty() ? "選ぶ..." : targetName_.c_str()))
	{
		layer->ForEachName([this, &changed](const std::string& name)
		{
			if (ImGui::Selectable(name.c_str(), name == targetName_))
			{
				targetName_ = name;
				changed = true;
			}
		});
		ImGui::EndCombo();
	}
	if (!targetName_.empty() && (!layer || !layer->Find(targetName_)))
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "見つからない");
	}
	return changed;
}
#endif
} // namespace KCE
