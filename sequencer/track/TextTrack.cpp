#include "sequencer/track/TextTrack.h"

#include <algorithm>
#include <cstdio>

#include "graphics/2d/TextOverlay.h"

namespace KCE
{
namespace
{
// 歌詞の出入りのフェード時間（秒）
constexpr float kFadeSeconds = 0.25f;
constexpr float kMinDuration = 0.1f;
constexpr float kMaxDuration = 60.0f;
constexpr float kMinCharsPerSecond = 1.0f;
constexpr float kMaxCharsPerSecond = 200.0f;
constexpr float kDragSpeed = 0.05f;
constexpr size_t kTextBufferSize = 1024;
constexpr size_t kSpeakerBufferSize = 128;
constexpr float kTextBoxHeight = 80.0f;

const char* KindToString(TextTrackKind kind)
{
	return kind == TextTrackKind::Dialogue ? "Dialogue" : "Lyric";
}

TextTrackKind KindFromString(const std::string& str)
{
	return str == "Dialogue" ? TextTrackKind::Dialogue : TextTrackKind::Lyric;
}
} // namespace

TextTrack::TextTrack()
{
	SetName("Text Track");
	// 出力先は画面に1つの TextOverlay なので役は使わない
	SetBindingRole("");
}

float TextTrack::GetEndTime() const
{
	float end = 0.0f;
	for (const TextEntry& entry : entries_)
	{
		end = (std::max)(end, entry.start + entry.duration);
	}
	return end;
}

const TextEntry* TextTrack::FindActiveEntry(float time) const
{
	const TextEntry* active = nullptr;
	for (const TextEntry& entry : entries_)
	{
		if (entry.start > time)
		{
			// 開始時刻順なので、以降はまだ始まっていない
			break;
		}
		if (time < entry.start + entry.duration)
		{
			active = &entry;
		}
	}
	return active;
}

void TextTrack::Evaluate(float time, const BindingContext& ctx)
{
	TextOverlay* overlay = ctx.GetTextOverlay();
	if (!overlay)
	{
		return;
	}

	const TextEntry* entry = FindActiveEntry(time);
	if (kind_ == TextTrackKind::Lyric)
	{
		if (!entry)
		{
			overlay->HideLyric();
			return;
		}
		// 行の頭と終わりでフェードする。どちらも行内の経過時間だけで決まる
		const float local = time - entry->start;
		const float fade = (std::min)({ 1.0f, local / kFadeSeconds, (entry->duration - local) / kFadeSeconds });
		overlay->ShowLyric(entry->text, (std::max)(fade, 0.0f));
		return;
	}

	if (!entry)
	{
		overlay->HideDialogue();
		return;
	}
	const float local = (std::max)(time - entry->start, 0.0f);
	const size_t visible = static_cast<size_t>(local * charsPerSecond_);
	overlay->ShowDialogue(entry->speaker, entry->text, visible, 1.0f);
}

void TextTrack::RestoreState(const BindingContext& ctx)
{
	TextOverlay* overlay = ctx.GetTextOverlay();
	if (!overlay)
	{
		return;
	}
	if (kind_ == TextTrackKind::Lyric)
	{
		overlay->HideLyric();
	}
	else
	{
		overlay->HideDialogue();
	}
}

size_t TextTrack::TextChannel::Insert(const TextEntry& entry)
{
	const auto it = std::upper_bound(entries_->begin(), entries_->end(), entry.start,
		[](float time, const TextEntry& e) { return time < e.start; });
	const size_t index = static_cast<size_t>(it - entries_->begin());
	entries_->insert(it, entry);
	return index;
}

size_t TextTrack::TextChannel::MoveKey(size_t index, float newTime)
{
	if (index >= entries_->size())
	{
		return index;
	}
	TextEntry entry = (*entries_)[index];
	entry.start = newTime;
	entries_->erase(entries_->begin() + index);
	return Insert(entry);
}

void TextTrack::TextChannel::RemoveKey(size_t index)
{
	if (index < entries_->size())
	{
		entries_->erase(entries_->begin() + index);
	}
}

bool TextTrack::TextChannel::AddKeyAt(float time)
{
	TextEntry entry;
	entry.start = time;
	Insert(entry);
	return true;
}

#ifdef USE_IMGUI
bool TextTrack::TextChannel::DrawKeyValueEditor(size_t index)
{
	TextEntry& entry = (*entries_)[index];
	bool changed = false;

	if (ImGui::DragFloat("表示時間", &entry.duration, kDragSpeed, kMinDuration, kMaxDuration, "%.2f s"))
	{
		changed = true;
	}

	if (*kind_ == TextTrackKind::Dialogue)
	{
		char speakerBuffer[kSpeakerBufferSize];
		std::snprintf(speakerBuffer, sizeof(speakerBuffer), "%s", entry.speaker.c_str());
		if (ImGui::InputText("話者", speakerBuffer, sizeof(speakerBuffer)))
		{
			entry.speaker = speakerBuffer;
			changed = true;
		}
	}

	char textBuffer[kTextBufferSize];
	std::snprintf(textBuffer, sizeof(textBuffer), "%s", entry.text.c_str());
	if (ImGui::InputTextMultiline("Text", textBuffer, sizeof(textBuffer), ImVec2(0.0f, kTextBoxHeight)))
	{
		entry.text = textBuffer;
		changed = true;
	}
	return changed;
}

bool TextTrack::DrawInspector()
{
	bool changed = false;
	int kind = static_cast<int>(kind_);
	if (ImGui::Combo("文字の種類", &kind, "歌詞\0会話\0"))
	{
		kind_ = static_cast<TextTrackKind>(kind);
		changed = true;
	}
	if (kind_ == TextTrackKind::Dialogue)
	{
		if (ImGui::DragFloat("1秒あたりの文字数", &charsPerSecond_, 0.5f, kMinCharsPerSecond, kMaxCharsPerSecond, "%.1f"))
		{
			changed = true;
		}
	}
	return changed;
}
#endif

bool TextTrack::RecordKey(float time, const BindingContext& ctx)
{
	(void)ctx;
	TextEntry entry;
	entry.start = time;
	if (kind_ == TextTrackKind::Dialogue)
	{
		entry.speaker = "Speaker";
		entry.text = "セリフ";
	}
	else
	{
		entry.text = "歌詞";
	}
	channel_.Insert(entry);
	return true;
}

nlohmann::json TextTrack::Serialize() const
{
	nlohmann::json json;
	SerializeCommon(json);
	json["kind"] = KindToString(kind_);
	json["charsPerSecond"] = charsPerSecond_;

	nlohmann::json entriesJson = nlohmann::json::array();
	for (const TextEntry& entry : entries_)
	{
		nlohmann::json entryJson;
		entryJson["start"] = entry.start;
		entryJson["duration"] = entry.duration;
		entryJson["speaker"] = entry.speaker;
		entryJson["text"] = entry.text;
		entriesJson.push_back(entryJson);
	}
	json["entries"] = entriesJson;
	return json;
}

bool TextTrack::Deserialize(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}

	DeserializeCommon(json);
	if (json.contains("kind") && json["kind"].is_string()) { kind_ = KindFromString(json["kind"].get<std::string>()); }
	if (json.contains("charsPerSecond") && json["charsPerSecond"].is_number()) { charsPerSecond_ = json["charsPerSecond"].get<float>(); }

	entries_.clear();
	if (json.contains("entries") && json["entries"].is_array())
	{
		for (const auto& entryJson : json["entries"])
		{
			if (!entryJson.is_object() || !entryJson.contains("start") || !entryJson["start"].is_number())
			{
				continue;
			}
			TextEntry entry;
			entry.start = entryJson["start"].get<float>();
			if (entryJson.contains("duration") && entryJson["duration"].is_number()) { entry.duration = entryJson["duration"].get<float>(); }
			if (entryJson.contains("speaker") && entryJson["speaker"].is_string()) { entry.speaker = entryJson["speaker"].get<std::string>(); }
			if (entryJson.contains("text") && entryJson["text"].is_string()) { entry.text = entryJson["text"].get<std::string>(); }
			channel_.Insert(entry);
		}
	}
	return true;
}
} // namespace KCE
