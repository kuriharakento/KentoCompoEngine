#include "sequencer/track/Text3DTrack.h"

#include <cstdio>

#include "graphics/text/Text3DRenderer.h"
#include "sequencer/core/CurveSerialization.h"

namespace KCE
{
namespace
{
constexpr size_t kNameBufferSize = 128;
constexpr size_t kTextBufferSize = 512;
constexpr float kTextBoxHeight = 60.0f;

} // namespace

Text3DTrack::Text3DTrack()
{
	SetName("Text3D Track");
	// 対象は名前で探すので役は使わない
	SetBindingRole("");
}

ICurveChannel* Text3DTrack::GetChannel(size_t index)
{
	switch (index)
	{
	case 0: return &positionChannel_;
	case 1: return &rotationChannel_;
	case 2: return &scaleChannel_;
	case 3: return &colorChannel_;
	case 4: return &revealChannel_;
	case 5: return &exitChannel_;
	default: return nullptr;
	}
}

TextMesh3D* Text3DTrack::ResolveMesh(const BindingContext& ctx) const
{
	const Text3DRenderer* renderer = ctx.GetText3DRenderer();
	return renderer ? renderer->Find(targetName_) : nullptr;
}

void Text3DTrack::Evaluate(float time, const BindingContext& ctx)
{
	TextMesh3D* mesh = ResolveMesh(ctx);
	if (!mesh)
	{
		return;
	}
	if (!text_.empty())
	{
		mesh->SetText(text_);
	}
	TextMesh3D::Params& params = mesh->GetParams();
	params.style = style_;
	// キーを持たないチャンネルには触れない
	if (!positionCurve_.IsEmpty()) { params.position = ctx.ApplyOriginToPoint(positionCurve_.Evaluate(time)); }
	if (!rotationCurve_.IsEmpty()) { params.rotation = ctx.ApplyOriginToRotation(rotationCurve_.Evaluate(time).Normalized()); }
	if (!scaleCurve_.IsEmpty()) { params.scale = scaleCurve_.Evaluate(time); }
	if (!colorCurve_.IsEmpty()) { params.color = colorCurve_.Evaluate(time); }
	if (!revealCurve_.IsEmpty()) { params.reveal = revealCurve_.Evaluate(time); }
	if (!exitCurve_.IsEmpty()) { params.exit = exitCurve_.Evaluate(time); }
}

void Text3DTrack::CaptureState(const BindingContext& ctx)
{
	hasCapturedState_ = false;
	const TextMesh3D* mesh = ResolveMesh(ctx);
	if (!mesh)
	{
		return;
	}
	capturedText_ = mesh->GetText();
	capturedParams_ = mesh->GetParams();
	hasCapturedState_ = true;
}

void Text3DTrack::RestoreState(const BindingContext& ctx)
{
	TextMesh3D* mesh = ResolveMesh(ctx);
	if (!hasCapturedState_ || !mesh)
	{
		return;
	}
	mesh->SetText(capturedText_);
	mesh->GetParams() = capturedParams_;
}

bool Text3DTrack::RecordKey(float time, const BindingContext& ctx)
{
	const TextMesh3D* mesh = ResolveMesh(ctx);
	if (!mesh)
	{
		return false;
	}
	const TextMesh3D::Params& params = mesh->GetParams();
	positionChannel_.SetKey(time, params.position);
	rotationChannel_.SetKey(time, params.rotation);
	scaleChannel_.SetKey(time, params.scale);
	colorChannel_.SetKey(time, params.color);
	// 全部出している状態は、文字数として記録する（大きな数のままだとカーブが扱いにくい）
	const float charCount = static_cast<float>(mesh->GetCharCount());
	revealChannel_.SetKey(time, params.reveal > charCount ? charCount : params.reveal);
	exitChannel_.SetKey(time, params.exit);
	return true;
}

nlohmann::json Text3DTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["target"] = targetName_;
	json["text"] = text_;
	json["style"] = TextAppearStyleToString(style_);
	json["position"] = SerializeCurve(positionCurve_);
	json["rotation"] = SerializeCurve(rotationCurve_);
	json["scale"] = SerializeCurve(scaleCurve_);
	json["color"] = SerializeCurve(colorCurve_);
	json["reveal"] = SerializeCurve(revealCurve_);
	json["exit"] = SerializeCurve(exitCurve_);
	return json;
}

bool Text3DTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}
	DeserializeCommon(json);
	if (json.contains("target") && json["target"].is_string()) { targetName_ = json["target"].get<std::string>(); }
	if (json.contains("text") && json["text"].is_string()) { text_ = json["text"].get<std::string>(); }
	if (json.contains("style") && json["style"].is_string()) { style_ = TextAppearStyleFromString(json["style"].get<std::string>()); }
	if (json.contains("position")) { DeserializeCurve(json["position"], positionCurve_); }
	if (json.contains("rotation")) { DeserializeCurve(json["rotation"], rotationCurve_); }
	if (json.contains("scale")) { DeserializeCurve(json["scale"], scaleCurve_); }
	if (json.contains("color")) { DeserializeCurve(json["color"], colorCurve_); }
	if (json.contains("reveal")) { DeserializeCurve(json["reveal"], revealCurve_); }
	if (json.contains("exit")) { DeserializeCurve(json["exit"], exitCurve_); }
	return true;
}

#ifdef USE_IMGUI
bool Text3DTrack::DrawInspector()
{
	BindingContext emptyContext;
	return DrawInspector(emptyContext);
}

bool Text3DTrack::DrawInspector(const BindingContext& ctx)
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
		ImGui::SetTooltip("Text3DRenderer に登録された名前（3D Text 窓に出ている名前）");
	}

	const Text3DRenderer* renderer = ctx.GetText3DRenderer();
	if (renderer && ImGui::BeginCombo("登録済み", targetName_.empty() ? "選ぶ..." : targetName_.c_str()))
	{
		renderer->ForEachName([this, &changed](const std::string& name)
		{
			const bool selected = name == targetName_;
			if (ImGui::Selectable(name.c_str(), selected))
			{
				targetName_ = name;
				changed = true;
			}
		});
		ImGui::EndCombo();
	}
	if (!targetName_.empty() && (!renderer || !renderer->Find(targetName_)))
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "見つからない");
	}

	char textBuffer[kTextBufferSize];
	std::snprintf(textBuffer, sizeof(textBuffer), "%s", text_.c_str());
	if (ImGui::InputTextMultiline("Text", textBuffer, sizeof(textBuffer), ImVec2(0.0f, kTextBoxHeight)))
	{
		text_ = textBuffer;
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("空なら対象の文字列をそのまま使う");
	}

	int style = static_cast<int>(style_);
	if (ImGui::Combo("出方", &style, "フェード\0落下\0回転\0ポップ\0"))
	{
		style_ = static_cast<TextAppearStyle>(style);
		changed = true;
	}
	return changed;
}
#endif
} // namespace KCE
