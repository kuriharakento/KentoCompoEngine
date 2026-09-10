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

float ITrack::GetChannelsEndTime()
{
	float end = 0.0f;
	for (size_t c = 0; c < GetChannelCount(); ++c)
	{
		ICurveChannel* channel = GetChannel(c);
		if (channel && !channel->IsEmpty())
		{
			// キーは時刻順に並んでいるので末尾が最大
			const float last = channel->GetKeyTime(channel->GetKeyCount() - 1);
			end = last > end ? last : end;
		}
	}
	return end;
}

bool ITrack::RemoveKeysAt(float time, float tolerance)
{
	bool removed = false;
	for (size_t c = 0; c < GetChannelCount(); ++c)
	{
		ICurveChannel* channel = GetChannel(c);
		if (!channel)
		{
			continue;
		}
		const int index = channel->FindKeyAt(time, tolerance);
		if (index >= 0)
		{
			channel->RemoveKey(static_cast<size_t>(index));
			removed = true;
		}
	}
	return removed;
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
