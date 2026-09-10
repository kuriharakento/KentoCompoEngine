#include "sequencer/core/ITrack.h"

namespace KCE
{
void ITrack::SerializeCommon(nlohmann::json& json) const
{
	json["type"] = GetTypeName();
	json["name"] = name_;
	json["binding"] = bindingRole_;
	json["muted"] = muted_;
}

void ITrack::DeserializeCommon(const nlohmann::json& json)
{
	// 欠けている項目は既定値のままにする。壊れたJSONで落とさないことを優先する。
	if (json.contains("name") && json["name"].is_string())
	{
		name_ = json["name"].get<std::string>();
	}
	if (json.contains("binding") && json["binding"].is_string())
	{
		bindingRole_ = json["binding"].get<std::string>();
	}
	if (json.contains("muted") && json["muted"].is_boolean())
	{
		muted_ = json["muted"].get<bool>();
	}
}
} // namespace KCE
