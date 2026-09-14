#pragma once

#include "math/Vector4.h"
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
class GameObject;
class JsonEditableBase;
namespace GameObjectComponent { class Component; }

/** @brief GameObject のコンポーネント項目と有効状態をカーブで駆動する。 */
class ComponentTrack : public ITrack
{
public:
	ComponentTrack();

	TrackType GetType() const override { return TrackType::Component; }
	const char* GetTypeName() const override { return "Component"; }
	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override { return const_cast<ComponentTrack*>(this)->GetChannelsEndTime(); }
	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;
	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;
	size_t GetChannelCount() const override;
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;

#ifdef USE_IMGUI
	/** @brief プレビュー対象があれば候補、なければ文字入力で設定を描く。 */
	bool DrawInspectorForObject(GameObject* object);
#endif

private:
	enum class ValueType
	{
		None,
		Float,
		Vector3,
		Vector4,
	};

	GameObjectComponent::Component* ResolveComponent(GameObject& object, JsonEditableBase*& outEditable) const;
	const GameObjectComponent::Component* ResolveComponent(const GameObject& object, const JsonEditableBase*& outEditable) const;
	void UpdateValueType(const nlohmann::json& value);
	void ClearWriteCache();

	std::string componentTypeName_;
	std::string propertyName_;
	ValueType valueType_ = ValueType::None;
	FloatCurve floatCurve_;
	Vector3Curve vector3Curve_;
	Vector4Curve vector4Curve_;
	FloatCurve enabledCurve_;
	CurveChannel<float> floatChannel_{ "Value", &floatCurve_ };
	CurveChannel<Vector3> vector3Channel_{ "Value", &vector3Curve_ };
	CurveChannel<Vector4> vector4Channel_{ "Value", &vector4Curve_ };
	CurveChannel<float> enabledChannel_{ "Enabled", &enabledCurve_ };

	bool hasCapturedState_ = false;
	nlohmann::json capturedValue_;
	bool capturedEnabled_ = true;
	bool hasLastWrittenValue_ = false;
	nlohmann::json lastWrittenValue_;
	bool hasLastWrittenEnabled_ = false;
	bool lastWrittenEnabled_ = true;
};
} // namespace KCE
