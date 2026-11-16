#include "RatioLink.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/component/base/ComponentFactory.h"
#include "engine/gameobject/component/render/GaugeComponent.h"

REGISTER_COMPONENT(RatioLink)
REGISTER_COMPONENT(RatioTimer)

namespace KCE
{
namespace GameObjectComponent
{
namespace
{
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

GameObject* FindByName(GameObject* root, const std::string& name)
{
	if (!root)
	{
		return nullptr;
	}
	if (root->GetName() == name)
	{
		return root;
	}
	for (const auto& child : root->GetChildren())
	{
		if (GameObject* found = FindByName(child.second.get(), name))
		{
			return found;
		}
	}
	return nullptr;
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
{
	return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
}

/** @brief 名前の型のコンポーネントのうち、編集できる（パラメータを持つ）ものを返す */
JsonEditableBase* FindEditable(GameObject* target, const std::string& typeName)
{
	if (!target)
	{
		return nullptr;
	}
	auto* factory = ComponentFactory::GetInstance();
	for (const auto& component : target->GetComponents())
	{
		if (factory->GetTypeName(typeid(*component)) == typeName)
		{
			return dynamic_cast<JsonEditableBase*>(component.get());
		}
	}
	return nullptr;
}

bool IsVectorJson(const nlohmann::json& value)
{
	return value.is_object() && value.contains("x") && value.contains("y") && value.contains("z");
}
} // namespace

void ForceLinkRatioLink() {}

RatioLink::RatioLink()
{
	Register("sourceName", &sourceName_);
	Register("targetName", &targetName_);
	Register("targetKind", &targetKind_);
	Register("componentType", &componentType_);
	Register("paramKey", &paramKey_);
	Register("valueAt0", &valueAt0_);
	Register("valueAt1", &valueAt1_);
	Register("useDisplayRatio", &useDisplayRatio_);
	Register("invertRatio", &invertRatio_);

#ifdef USE_IMGUI
	// 対象の種類・コンポーネント・パラメータは、文字や数字ではなくプルダウンで選ばせる
	OverrideDrawer("targetKind", [this]() {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("targetKind");
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-FLT_MIN);
		const char* kinds[] = { "位置", "回転（度）", "スケール", "コンポーネントのパラメータ" };
		ImGui::Combo("##kind", &targetKind_, kinds, IM_ARRAYSIZE(kinds));
		ImGui::PopItemWidth();
	});
	OverrideDrawer("componentType", [this]() {
		if (targetKind_ != kComponentParam)
		{
			return;
		}
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("componentType");
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##component", componentType_.c_str()))
		{
			if (GameObject* target = Resolve(targetName_))
			{
				auto* factory = ComponentFactory::GetInstance();
				for (const auto& component : target->GetComponents())
				{
					if (!dynamic_cast<JsonEditableBase*>(component.get()) || component.get() == this)
					{
						continue;
					}
					const std::string name = factory->GetTypeName(typeid(*component));
					if (ImGui::Selectable(name.c_str(), name == componentType_))
					{
						componentType_ = name;
						paramKey_.clear();
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	});
	OverrideDrawer("paramKey", [this]() {
		if (targetKind_ != kComponentParam)
		{
			return;
		}
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("paramKey");
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("##param", paramKey_.c_str()))
		{
			if (JsonEditableBase* editable = FindEditable(Resolve(targetName_), componentType_))
			{
				// 数値（float）か Vector3 のパラメータだけを並べる
				for (const auto& [key, value] : editable->Serialize().items())
				{
					if (!value.is_number() && !IsVectorJson(value))
					{
						continue;
					}
					if (ImGui::Selectable(key.c_str(), key == paramKey_))
					{
						paramKey_ = key;
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	});
#endif
}

GameObject* RatioLink::Resolve(const std::string& name) const
{
	GameObject* owner = GetOwner();
	if (name.empty())
	{
		return owner;
	}
	return FindByName(owner, name);
}

void RatioLink::LateUpdate()
{
	GameObject* source = Resolve(sourceName_);
	GameObject* target = Resolve(targetName_);
	if (!source || !target)
	{
		return;
	}
	const GaugeComponent* gauge = source->GetComponent<GaugeComponent>();
	if (!gauge)
	{
		return;
	}
	float ratio = std::clamp(useDisplayRatio_ ? gauge->GetDisplayRatio() : gauge->GetRatio(), 0.0f, 1.0f);
	if (invertRatio_)
	{
		ratio = 1.0f - ratio;
	}
	const Vector3 value = Lerp(valueAt0_, valueAt1_, ratio);

	switch (targetKind_)
	{
	case kPosition:
		target->SetPosition(value);
		break;
	case kRotationDeg:
		target->SetRotation(value * kDegToRad);
		break;
	case kScale:
		target->SetScale(value);
		break;
	case kComponentParam:
	{
		JsonEditableBase* editable = FindEditable(target, componentType_);
		if (!editable || paramKey_.empty())
		{
			return;
		}
		// 登録パラメータの型（float か Vector3）は、いまの値から見分ける
		const nlohmann::json current = editable->Serialize();
		if (!current.contains(paramKey_))
		{
			return;
		}
		if (IsVectorJson(current[paramKey_]))
		{
			editable->SetValue(paramKey_, nlohmann::json(value));
		}
		else if (current[paramKey_].is_number())
		{
			editable->SetValue(paramKey_, value.x);
		}
		break;
	}
	default:
		break;
	}
}

RatioTimer::RatioTimer()
{
	Register("sourceName", &sourceName_);
	Register("duration", &duration_);
	Register("startRatio", &startRatio_);
	Register("endRatio", &endRatio_);
	Register("loop", &loop_);
}

void RatioTimer::Update()
{
	GameObject* owner = GetOwner();
	GameObject* source = sourceName_.empty() ? owner : FindByName(owner, sourceName_);
	GaugeComponent* gauge = source ? source->GetComponent<GaugeComponent>() : nullptr;
	if (!gauge)
	{
		return;
	}
	elapsed_ += GetDeltaTime();
	float t = duration_ > 0.0f ? elapsed_ / duration_ : 1.0f;
	if (t >= 1.0f)
	{
		if (loop_ && duration_ > 0.0f)
		{
			elapsed_ = std::fmod(elapsed_, duration_);
			t = elapsed_ / duration_;
			// 巻き戻すときは追従を待たず、最初の値へ戻す
			gauge->SetRatioImmediate(startRatio_);
			return;
		}
		t = 1.0f;
	}
	gauge->SetRatio(startRatio_ + (endRatio_ - startRatio_) * t);
}

#ifdef USE_IMGUI
void RatioLink::DrawImGui()
{
	JsonEditableBase::DrawImGui();
	if (!Resolve(sourceName_) || !Resolve(sourceName_)->GetComponent<GaugeComponent>())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "sourceName の GameObject に GaugeComponent が見つからない");
	}
	if (!Resolve(targetName_))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "targetName の GameObject が見つからない");
	}
	ImGui::TextDisabled("floatのパラメータは valueAt の x だけを使う。回転は度");
}
#endif
} // namespace GameObjectComponent
} // namespace KCE
