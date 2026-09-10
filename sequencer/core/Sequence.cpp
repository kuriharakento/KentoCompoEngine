#include "sequencer/core/Sequence.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "base/Logger.h"
#include "base/PathManager.h"
#include "core/SchemaVersion.h"
#include "sequencer/core/TrackFactory.h"

namespace KCE
{
namespace
{
/** @brief シーケンスJSONを置くディレクトリ（アプリのリソースルートからの相対） */
const char* const kSequenceDirectory = "json/sequence";

/**
 * @brief 保存・読み込み先のフルパスを解決する
 * @param path 絶対パス、またはシーケンスディレクトリからの相対パス
 * @return 解決されたパス
 */
std::filesystem::path ResolveSequencePath(const std::string& path)
{
	std::filesystem::path candidate(path);
	if (candidate.is_absolute())
	{
		return candidate;
	}
	return PathManager::GetApplicationResourceRoot() / kSequenceDirectory / candidate;
}

/**
 * @brief バインディング種別を文字列に変換する
 */
const char* BindingTypeToString(BindingType type)
{
	switch (type)
	{
	case BindingType::Camera: return "Camera";
	case BindingType::Light:  return "Light";
	default:                  return "GameObject";
	}
}

/**
 * @brief 文字列からバインディング種別を復元する
 */
BindingType BindingTypeFromString(const std::string& str)
{
	if (str == "Camera") { return BindingType::Camera; }
	if (str == "Light") { return BindingType::Light; }
	return BindingType::GameObject;
}
} // namespace

void Sequence::Evaluate(float time, const BindingContext& ctx)
{
	for (const auto& track : tracks_)
	{
		if (!track || track->IsMuted())
		{
			continue;
		}
		track->Evaluate(time, ctx);
	}
}

void Sequence::CaptureState(const BindingContext& ctx)
{
	for (const auto& track : tracks_)
	{
		if (track)
		{
			track->CaptureState(ctx);
		}
	}
}

void Sequence::RestoreState(const BindingContext& ctx)
{
	// 適用した逆順に戻す。複数トラックが同じ対象を触っていた場合に、
	// 最初に退避された状態が最終的に残るようにするため。
	for (auto it = tracks_.rbegin(); it != tracks_.rend(); ++it)
	{
		if (*it)
		{
			(*it)->RestoreState(ctx);
		}
	}
}

ITrack* Sequence::AddTrack(TrackPtr track)
{
	if (!track)
	{
		return nullptr;
	}

	ITrack* raw = track.get();
	tracks_.push_back(std::move(track));
	return raw;
}

TrackPtr Sequence::DetachTrack(size_t index)
{
	if (index >= tracks_.size())
	{
		return nullptr;
	}

	TrackPtr track = std::move(tracks_[index]);
	tracks_.erase(tracks_.begin() + index);
	return track;
}

void Sequence::InsertTrack(size_t index, TrackPtr track)
{
	if (!track)
	{
		return;
	}

	if (index >= tracks_.size())
	{
		tracks_.push_back(std::move(track));
		return;
	}
	tracks_.insert(tracks_.begin() + index, std::move(track));
}

ITrack* Sequence::GetTrack(size_t index) const
{
	return index < tracks_.size() ? tracks_[index].get() : nullptr;
}

void Sequence::AddBinding(const BindingDefinition& definition)
{
	const auto it = std::find_if(bindings_.begin(), bindings_.end(),
		[&definition](const BindingDefinition& existing) { return existing.role == definition.role; });
	if (it != bindings_.end())
	{
		return;
	}
	bindings_.push_back(definition);
}

float Sequence::GetDuration() const
{
	if (meta_.duration > 0.0f)
	{
		return meta_.duration;
	}

	float end = 0.0f;
	for (const auto& track : tracks_)
	{
		if (track)
		{
			end = (std::max)(end, track->GetEndTime());
		}
	}
	return end;
}

nlohmann::json Sequence::Serialize() const
{
	nlohmann::json json;

	// バージョンは必ず先頭に置く（SEQUENCER_PLAN 3.4）
	json["version"] = kSequenceSchemaVersion;

	nlohmann::json metaJson;
	metaJson["name"] = meta_.name;
	metaJson["duration"] = GetDuration();
	metaJson["bpm"] = meta_.bpm;
	metaJson["offset"] = meta_.offset;
	metaJson["audioClip"] = meta_.audioClip;
	json["meta"] = metaJson;

	nlohmann::json bindingsJson = nlohmann::json::array();
	for (const auto& binding : bindings_)
	{
		nlohmann::json bindingJson;
		bindingJson["role"] = binding.role;
		bindingJson["type"] = BindingTypeToString(binding.type);
		bindingJson["description"] = binding.description;
		bindingsJson.push_back(bindingJson);
	}
	json["bindings"] = bindingsJson;

	nlohmann::json tracksJson = nlohmann::json::array();
	for (const auto& track : tracks_)
	{
		if (track)
		{
			tracksJson.push_back(track->Serialize());
		}
	}
	json["tracks"] = tracksJson;

	nlohmann::json markersJson = nlohmann::json::array();
	for (const auto& marker : markers_)
	{
		nlohmann::json markerJson;
		markerJson["time"] = marker.time;
		markerJson["name"] = marker.name;
		markersJson.push_back(markerJson);
	}
	json["markers"] = markersJson;

	return json;
}

bool Sequence::Deserialize(const nlohmann::json& json, std::string* outError)
{
	if (!json.is_object())
	{
		if (outError) { *outError = "ルートがオブジェクトではありません"; }
		return false;
	}

	// バージョンを持たないデータは、バージョン1相当の旧データとして扱う
	const int version = (json.contains("version") && json["version"].is_number_integer())
		? json["version"].get<int>()
		: 1;

	if (!IsLoadableSchemaVersion(version, kSequenceSchemaVersion))
	{
		if (outError)
		{
			*outError = "対応していないスキーマバージョンです (file=" + std::to_string(version) + ", engine=" + std::to_string(kSequenceSchemaVersion) + ")";
		}
		return false;
	}

	Clear();

	if (json.contains("meta") && json["meta"].is_object())
	{
		const auto& metaJson = json["meta"];
		if (metaJson.contains("name") && metaJson["name"].is_string()) { meta_.name = metaJson["name"].get<std::string>(); }
		if (metaJson.contains("duration") && metaJson["duration"].is_number()) { meta_.duration = metaJson["duration"].get<float>(); }
		if (metaJson.contains("bpm") && metaJson["bpm"].is_number()) { meta_.bpm = metaJson["bpm"].get<float>(); }
		if (metaJson.contains("offset") && metaJson["offset"].is_number()) { meta_.offset = metaJson["offset"].get<float>(); }
		if (metaJson.contains("audioClip") && metaJson["audioClip"].is_string()) { meta_.audioClip = metaJson["audioClip"].get<std::string>(); }
	}

	if (json.contains("bindings") && json["bindings"].is_array())
	{
		for (const auto& bindingJson : json["bindings"])
		{
			if (!bindingJson.is_object() || !bindingJson.contains("role")) { continue; }

			BindingDefinition binding;
			binding.role = bindingJson["role"].get<std::string>();
			if (bindingJson.contains("type") && bindingJson["type"].is_string())
			{
				binding.type = BindingTypeFromString(bindingJson["type"].get<std::string>());
			}
			if (bindingJson.contains("description") && bindingJson["description"].is_string())
			{
				binding.description = bindingJson["description"].get<std::string>();
			}
			bindings_.push_back(binding);
		}
	}

	if (json.contains("tracks") && json["tracks"].is_array())
	{
		for (const auto& trackJson : json["tracks"])
		{
			if (!trackJson.is_object() || !trackJson.contains("type") || !trackJson["type"].is_string())
			{
				continue;
			}

			const std::string typeName = trackJson["type"].get<std::string>();
			TrackPtr track = TrackFactory::Create(typeName);
			if (!track)
			{
				// 未知の種別は読み飛ばす。将来のエンジンで保存したデータを
				// 古いエンジンで開いても、他のトラックは編集できるようにするため。
				Logger::Log("Sequence: 未知のトラック種別を読み飛ばしました: " + typeName + "\n", Logger::LogLevel::Warning);
				continue;
			}

			if (track->Deserialize(trackJson))
			{
				tracks_.push_back(std::move(track));
			}
		}
	}

	if (json.contains("markers") && json["markers"].is_array())
	{
		for (const auto& markerJson : json["markers"])
		{
			if (!markerJson.is_object() || !markerJson.contains("time")) { continue; }

			SequenceMarker marker;
			marker.time = markerJson["time"].get<float>();
			if (markerJson.contains("name") && markerJson["name"].is_string())
			{
				marker.name = markerJson["name"].get<std::string>();
			}
			markers_.push_back(marker);
		}
	}

	return true;
}

bool Sequence::SaveToFile(const std::string& path) const
{
	const std::filesystem::path fullPath = ResolveSequencePath(path);

	std::error_code ec;
	std::filesystem::create_directories(fullPath.parent_path(), ec);

	// 一時ファイルに書いてからリネームする。
	// 直接上書きすると、書き込み中に落ちたときに何十時間もかけた演出データが失われる。
	const std::filesystem::path tempPath = fullPath.string() + ".tmp";
	{
		std::ofstream ofs(tempPath);
		if (!ofs)
		{
			Logger::Log("Sequence: 保存先を開けませんでした: " + tempPath.string() + "\n", Logger::LogLevel::Error);
			return false;
		}
		ofs << Serialize().dump(4);
		if (!ofs)
		{
			Logger::Log("Sequence: 書き込みに失敗しました: " + tempPath.string() + "\n", Logger::LogLevel::Error);
			return false;
		}
	}

	std::filesystem::rename(tempPath, fullPath, ec);
	if (ec)
	{
		// 同名が既に存在する環境ではリネームが失敗しうるので、置換で再試行する
		std::filesystem::remove(fullPath, ec);
		std::filesystem::rename(tempPath, fullPath, ec);
		if (ec)
		{
			Logger::Log("Sequence: 保存の確定に失敗しました: " + ec.message() + "\n", Logger::LogLevel::Error);
			return false;
		}
	}

	return true;
}

bool Sequence::LoadFromFile(const std::string& path, std::string* outError)
{
	const std::filesystem::path fullPath = ResolveSequencePath(path);

	if (!std::filesystem::exists(fullPath))
	{
		if (outError) { *outError = "ファイルが見つかりません: " + fullPath.string(); }
		return false;
	}

	std::ifstream ifs(fullPath);
	if (!ifs)
	{
		if (outError) { *outError = "ファイルを開けません: " + fullPath.string(); }
		return false;
	}

	nlohmann::json json;
	try
	{
		ifs >> json;
	}
	catch (const nlohmann::json::exception& e)
	{
		// 壊れたJSONを開くのはエディタでは日常的に起きる。assertで落とさない。
		if (outError) { *outError = std::string("JSONの解析に失敗しました: ") + e.what(); }
		return false;
	}

	return Deserialize(json, outError);
}

void Sequence::Clear()
{
	tracks_.clear();
	markers_.clear();
	bindings_.clear();
	meta_ = SequenceMeta{};
}
} // namespace KCE
