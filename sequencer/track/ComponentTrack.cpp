#include "sequencer/track/ComponentTrack.h"

#include <cstdio>

#include "gameobject/base/GameObject.h"
#include "gameobject/component/base/Component.h"
#include "jsonEditor/JsonEditableBase.h"
#include "sequencer/core/CurveSerialization.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
namespace
{
const char* ValueTypeToString(int type)
{
	switch (type)
	{
	case 1: return "Float";
	case 2: return "Vector3";
	case 3: return "Vector4";
	default: return "None";
	}
}

int ValueTypeFromString(const std::string& type)
{
	if (type == "Float") { return 1; }
	if (type == "Vector3") { return 2; }
	if (type == "Vector4") { return 3; }
	return 0;
}

bool IsVectorValue(const nlohmann::json& value, size_t size)
{
	if (value.is_array()) { return value.size() == size; }
	if (!value.is_object()) { return false; }
	return value.contains("x") && value.contains("y") && value.contains("z") &&
		((size == 3 && !value.contains("w")) || (size == 4 && value.contains("w")));
}
} // namespace

ComponentTrack::ComponentTrack()
{
	SetName("Component Track");
	SetBindingRole("Target");
}

GameObjectComponent::Component* ComponentTrack::ResolveComponent(GameObject& object, JsonEditableBase*& outEditable) const
{
	outEditable = nullptr;
	const auto& names = object.GetComponentTypeNames();
	const auto& components = object.GetComponents();
	for (size_t index = 0; index < names.size() && index < components.size(); ++index)
	{
		if (names[index] == componentTypeName_)
		{
			outEditable = dynamic_cast<JsonEditableBase*>(components[index].get());
			return components[index].get();
		}
	}
	return nullptr;
}

const GameObjectComponent::Component* ComponentTrack::ResolveComponent(
	const GameObject& object, const JsonEditableBase*& outEditable) const
{
	outEditable = nullptr;
	const auto& names = object.GetComponentTypeNames();
	const auto& components = object.GetComponents();
	for (size_t index = 0; index < names.size() && index < components.size(); ++index)
	{
		if (names[index] == componentTypeName_)
		{
			outEditable = dynamic_cast<const JsonEditableBase*>(components[index].get());
			return components[index].get();
		}
	}
	return nullptr;
}

void ComponentTrack::UpdateValueType(const nlohmann::json& value)
{
	if (value.is_number()) { valueType_ = ValueType::Float; }
	else if (IsVectorValue(value, 3)) { valueType_ = ValueType::Vector3; }
	else if (IsVectorValue(value, 4)) { valueType_ = ValueType::Vector4; }
	else { valueType_ = ValueType::None; }
	ClearWriteCache();
}

void ComponentTrack::ClearWriteCache()
{
	hasLastWrittenValue_ = false;
	hasLastWrittenEnabled_ = false;
}

size_t ComponentTrack::GetChannelCount() const
{
	return valueType_ == ValueType::None ? 1 : 2;
}

ICurveChannel* ComponentTrack::GetChannel(size_t index)
{
	if (valueType_ == ValueType::None) { return index == 0 ? &enabledChannel_ : nullptr; }
	if (index == 1) { return &enabledChannel_; }
	if (index != 0) { return nullptr; }
	switch (valueType_)
	{
	case ValueType::Float:   return &floatChannel_;
	case ValueType::Vector3: return &vector3Channel_;
	case ValueType::Vector4: return &vector4Channel_;
	default:                 return nullptr;
	}
}

void ComponentTrack::Evaluate(float time, const BindingContext& ctx)
{
	GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object) { return; }
	JsonEditableBase* editable = nullptr;
	GameObjectComponent::Component* component = ResolveComponent(*object, editable);
	if (!component) { return; }

	if (editable && !propertyName_.empty())
	{
		nlohmann::json value;
		bool hasValue = true;
		switch (valueType_)
		{
		case ValueType::Float:   if (!floatCurve_.IsEmpty()) { value = floatCurve_.Evaluate(time); } else { hasValue = false; } break;
		case ValueType::Vector3: if (!vector3Curve_.IsEmpty()) { value = vector3Curve_.Evaluate(time); } else { hasValue = false; } break;
		case ValueType::Vector4: if (!vector4Curve_.IsEmpty()) { value = vector4Curve_.Evaluate(time); } else { hasValue = false; } break;
		default: hasValue = false; break;
		}
		if (hasValue && (!hasLastWrittenValue_ || lastWrittenValue_ != value))
		{
			editable->SetValue(propertyName_, value);
			lastWrittenValue_ = std::move(value);
			hasLastWrittenValue_ = true;
		}
	}

	if (!enabledCurve_.IsEmpty())
	{
		constexpr float kEnabledThreshold = 0.5f;
		const bool enabled = enabledCurve_.Evaluate(time) >= kEnabledThreshold;
		if (!hasLastWrittenEnabled_ || lastWrittenEnabled_ != enabled)
		{
			component->SetEnabled(enabled);
			lastWrittenEnabled_ = enabled;
			hasLastWrittenEnabled_ = true;
		}
	}
}

void ComponentTrack::CaptureState(const BindingContext& ctx)
{
	hasCapturedState_ = false;
	capturedValue_ = nullptr;
	ClearWriteCache();
	const GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object) { return; }
	const JsonEditableBase* editable = nullptr;
	const GameObjectComponent::Component* component = ResolveComponent(*object, editable);
	if (!component) { return; }
	if (editable)
	{
		const nlohmann::json fields = editable->Serialize();
		if (fields.contains(propertyName_)) { capturedValue_ = fields[propertyName_]; }
	}
	capturedEnabled_ = component->IsEnabled();
	hasCapturedState_ = true;
}

void ComponentTrack::RestoreState(const BindingContext& ctx)
{
	if (!hasCapturedState_) { return; }
	GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object) { return; }
	JsonEditableBase* editable = nullptr;
	GameObjectComponent::Component* component = ResolveComponent(*object, editable);
	if (!component) { return; }
	if (editable && !capturedValue_.is_null()) { editable->SetValue(propertyName_, capturedValue_); }
	component->SetEnabled(capturedEnabled_);
	ClearWriteCache();
}

bool ComponentTrack::RecordKey(float time, const BindingContext& ctx)
{
	const GameObject* object = ctx.GetGameObject(GetBindingRole());
	if (!object) { return false; }
	const JsonEditableBase* editable = nullptr;
	const GameObjectComponent::Component* component = ResolveComponent(*object, editable);
	if (!component) { return false; }

	bool recordedValue = false;
	if (editable)
	{
		const nlohmann::json fields = editable->Serialize();
		const auto it = fields.find(propertyName_);
		if (it != fields.end())
		{
			UpdateValueType(*it);
			switch (valueType_)
			{
			case ValueType::Float:   floatChannel_.SetKey(time, it->get<float>()); recordedValue = true; break;
			case ValueType::Vector3: vector3Channel_.SetKey(time, it->get<Vector3>()); recordedValue = true; break;
			case ValueType::Vector4: vector4Channel_.SetKey(time, it->get<Vector4>()); recordedValue = true; break;
			default: break;
			}
		}
	}

	enabledChannel_.SetKey(time, component->IsEnabled() ? 1.0f : 0.0f);
	const int enabledKey = enabledChannel_.FindKeyAt(time, 0.001f);
	if (enabledKey >= 0)
	{
		enabledChannel_.GetKeyInterp(static_cast<size_t>(enabledKey)) = InterpolationMode::Constant;
	}
	return recordedValue || !enabledCurve_.IsEmpty();
}

nlohmann::json ComponentTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["componentType"] = componentTypeName_;
	json["property"] = propertyName_;
	json["valueType"] = ValueTypeToString(static_cast<int>(valueType_));
	json["floatValue"] = SerializeCurve(floatCurve_);
	json["vector3Value"] = SerializeCurve(vector3Curve_);
	json["vector4Value"] = SerializeCurve(vector4Curve_);
	json["enabled"] = SerializeCurve(enabledCurve_);
	return json;
}

bool ComponentTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object()) { return false; }
	DeserializeCommon(json);
	if (json.contains("componentType") && json["componentType"].is_string()) { componentTypeName_ = json["componentType"].get<std::string>(); }
	if (json.contains("property") && json["property"].is_string()) { propertyName_ = json["property"].get<std::string>(); }
	if (json.contains("valueType") && json["valueType"].is_string())
	{
		valueType_ = static_cast<ValueType>(ValueTypeFromString(json["valueType"].get<std::string>()));
	}
	if (json.contains("floatValue")) { DeserializeCurve(json["floatValue"], floatCurve_); }
	if (json.contains("vector3Value")) { DeserializeCurve(json["vector3Value"], vector3Curve_); }
	if (json.contains("vector4Value")) { DeserializeCurve(json["vector4Value"], vector4Curve_); }
	if (json.contains("enabled")) { DeserializeCurve(json["enabled"], enabledCurve_); }
	ClearWriteCache();
	return true;
}

#ifdef USE_IMGUI
bool ComponentTrack::DrawInspectorForObject(GameObject* object)
{
	bool changed = false;
	if (!object)
	{
		char componentBuffer[128];
		char propertyBuffer[128];
		std::snprintf(componentBuffer, sizeof(componentBuffer), "%s", componentTypeName_.c_str());
		std::snprintf(propertyBuffer, sizeof(propertyBuffer), "%s", propertyName_.c_str());
		if (ImGui::InputText("Component Type", componentBuffer, sizeof(componentBuffer)))
		{
			componentTypeName_ = componentBuffer;
			changed = true;
		}
		if (ImGui::InputText("Property", propertyBuffer, sizeof(propertyBuffer)))
		{
			propertyName_ = propertyBuffer;
			valueType_ = ValueType::None;
			changed = true;
		}
		return changed;
	}

	const auto& names = object->GetComponentTypeNames();
	const auto& components = object->GetComponents();
	if (ImGui::BeginCombo("Component Type", componentTypeName_.empty() ? "(選択)" : componentTypeName_.c_str()))
	{
		for (size_t index = 0; index < names.size() && index < components.size(); ++index)
		{
			if (ImGui::Selectable(names[index].c_str(), names[index] == componentTypeName_))
			{
				componentTypeName_ = names[index];
				propertyName_.clear();
				valueType_ = ValueType::None;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}

	JsonEditableBase* editable = nullptr;
	ResolveComponent(*object, editable);
	if (editable && ImGui::BeginCombo("Property", propertyName_.empty() ? "(選択)" : propertyName_.c_str()))
	{
		const nlohmann::json fields = editable->Serialize();
		for (auto it = fields.begin(); it != fields.end(); ++it)
		{
			if (!it.value().is_number() && !IsVectorValue(it.value(), 3) && !IsVectorValue(it.value(), 4)) { continue; }
			if (ImGui::Selectable(it.key().c_str(), it.key() == propertyName_))
			{
				propertyName_ = it.key();
				UpdateValueType(it.value());
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}
#endif
} // namespace KCE
