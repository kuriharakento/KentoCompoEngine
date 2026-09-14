#include "sequencer/track/EventTrack.h"

#include <algorithm>
#include <cstdio>

namespace KCE
{
EventTrack::EventTrack()
{
	SetName("Event Track");
	// イベントはゲーム側のコールバックに渡すだけなので役は使わない
	SetBindingRole("");
}

float EventTrack::GetEndTime() const
{
	return events_.empty() ? 0.0f : events_.back().time;
}

size_t EventTrack::EventChannel::Insert(const SequenceEvent& event)
{
	const auto it = std::upper_bound(events_->begin(), events_->end(), event.time,
		[](float time, const SequenceEvent& e) { return time < e.time; });
	const size_t index = static_cast<size_t>(it - events_->begin());
	events_->insert(it, event);
	return index;
}

size_t EventTrack::EventChannel::MoveKey(size_t index, float newTime)
{
	if (index >= events_->size())
	{
		return index;
	}
	SequenceEvent event = (*events_)[index];
	event.time = newTime;
	events_->erase(events_->begin() + index);
	return Insert(event);
}

void EventTrack::EventChannel::RemoveKey(size_t index)
{
	if (index < events_->size())
	{
		events_->erase(events_->begin() + index);
	}
}

nlohmann::json EventTrack::EventChannel::CopyKey(size_t index) const
{
	if (index >= events_->size())
	{
		return nullptr;
	}
	const SequenceEvent& event = (*events_)[index];
	nlohmann::json json;
	json["name"] = event.name;
	json["fireOnSkip"] = event.fireOnSkip;
	return json;
}

bool EventTrack::EventChannel::PasteKey(float time, const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}
	SequenceEvent event;
	event.time = time;
	if (json.contains("name") && json["name"].is_string()) { event.name = json["name"].get<std::string>(); }
	if (json.contains("fireOnSkip") && json["fireOnSkip"].is_boolean()) { event.fireOnSkip = json["fireOnSkip"].get<bool>(); }
	Insert(event);
	return true;
}

bool EventTrack::EventChannel::AddKeyAt(float time)
{
	SequenceEvent event;
	event.time = time;
	event.name = "Event" + std::to_string(events_->size());
	Insert(event);
	return true;
}

#ifdef USE_IMGUI
bool EventTrack::EventChannel::DrawKeyValueEditor(size_t index)
{
	SequenceEvent& event = (*events_)[index];
	bool changed = false;

	char nameBuffer[128];
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", event.name.c_str());
	if (ImGui::InputText("イベント名", nameBuffer, sizeof(nameBuffer)))
	{
		event.name = nameBuffer;
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("ゲーム側のコールバックに渡される名前");
	}

	if (ImGui::Checkbox("スキップ時も呼ぶ", &event.fireOnSkip))
	{
		changed = true;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("スキップしたときにも呼ぶか。\n進行に必要なもの（敵を出す等）はオン、\n演出だけのもの（効果音等）はオフにする");
	}
	return changed;
}
#endif

bool EventTrack::RecordKey(float time, const BindingContext& ctx)
{
	(void)ctx;
	SequenceEvent event;
	event.time = time;
	event.name = "Event" + std::to_string(events_.size());
	channel_.Insert(event);
	return true;
}

void EventTrack::ForEachEventInRange(float from, float to, bool skipping, const std::function<void(const SequenceEvent&)>& callback) const
{
	for (const SequenceEvent& event : events_)
	{
		if (event.time <= from)
		{
			continue;
		}
		if (event.time > to)
		{
			// 時刻順に並んでいるので、以降はすべて範囲外
			break;
		}
		if (skipping && !event.fireOnSkip)
		{
			continue;
		}
		callback(event);
	}
}

nlohmann::json EventTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);

	nlohmann::json eventsJson = nlohmann::json::array();
	for (const SequenceEvent& event : events_)
	{
		nlohmann::json eventJson;
		eventJson["time"] = event.time;
		eventJson["name"] = event.name;
		eventJson["fireOnSkip"] = event.fireOnSkip;
		eventsJson.push_back(eventJson);
	}
	json["events"] = eventsJson;
	return json;
}

bool EventTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}

	DeserializeCommon(json);
	events_.clear();

	if (json.contains("events") && json["events"].is_array())
	{
		for (const auto& eventJson : json["events"])
		{
			if (!eventJson.is_object() || !eventJson.contains("time") || !eventJson["time"].is_number())
			{
				continue;
			}

			SequenceEvent event;
			event.time = eventJson["time"].get<float>();
			if (eventJson.contains("name") && eventJson["name"].is_string()) { event.name = eventJson["name"].get<std::string>(); }
			if (eventJson.contains("fireOnSkip") && eventJson["fireOnSkip"].is_boolean()) { event.fireOnSkip = eventJson["fireOnSkip"].get<bool>(); }
			channel_.Insert(event);
		}
	}
	return true;
}
} // namespace KCE
