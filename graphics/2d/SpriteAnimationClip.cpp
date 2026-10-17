#include "SpriteAnimationClip.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "base/PathManager.h"

namespace KCE
{
namespace
{
constexpr const char* kKeyKeys = "keys";
constexpr const char* kKeyLoop = "isLooping";
constexpr const char* kSaveDirectory = "spriteAnimations";
constexpr int kJsonIndent = 4;
constexpr int kNoKey = -1;
} // namespace

size_t SpriteAnimationClip::AddKey(float time, const std::string& texturePath)
{
	SpriteAnimationKey key;
	key.time = (std::max)(time, 0.0f);
	key.texturePath = texturePath;

	// 同時刻なら後ろに入れる（upper_bound）
	auto it = std::upper_bound(keys_.begin(), keys_.end(), key.time,
		[](float t, const SpriteAnimationKey& k) { return t < k.time; });
	it = keys_.insert(it, std::move(key));
	return static_cast<size_t>(it - keys_.begin());
}

void SpriteAnimationClip::RemoveKey(size_t index)
{
	if (index >= keys_.size())
	{
		return;
	}
	keys_.erase(keys_.begin() + static_cast<std::ptrdiff_t>(index));
}

size_t SpriteAnimationClip::MoveKey(size_t index, float newTime)
{
	if (index >= keys_.size())
	{
		return index;
	}

	// 取り出して入れ直すだけで、時刻順が保たれる
	const std::string texturePath = keys_[index].texturePath;
	RemoveKey(index);
	return AddKey(newTime, texturePath);
}

void SpriteAnimationClip::SetKeyTexture(size_t index, const std::string& texturePath)
{
	if (index >= keys_.size())
	{
		return;
	}
	keys_[index].texturePath = texturePath;
}

float SpriteAnimationClip::WrapTime(float time) const
{
	const float duration = GetDuration();
	time = (std::max)(time, 0.0f);
	if (duration <= 0.0f)
	{
		return 0.0f;
	}
	if (isLooping_)
	{
		return std::fmod(time, duration);
	}
	return (std::min)(time, duration);
}

int SpriteAnimationClip::FindKeyIndex(float time) const
{
	if (keys_.empty())
	{
		return kNoKey;
	}

	// time 以前で一番後ろのキー。最初のキーより前なら最初のキーを出す
	const float wrapped = WrapTime(time);
	const auto it = std::upper_bound(keys_.begin(), keys_.end(), wrapped,
		[](float t, const SpriteAnimationKey& k) { return t < k.time; });
	if (it == keys_.begin())
	{
		return 0;
	}
	return static_cast<int>(it - keys_.begin()) - 1;
}

nlohmann::json SpriteAnimationClip::Serialize() const
{
	// キーの並びはマクロが作った to_json に任せる
	return { { kKeyKeys, keys_ }, { kKeyLoop, isLooping_ } };
}

bool SpriteAnimationClip::Deserialize(const nlohmann::json& json)
{
	// 一時オブジェクトに読んでから差し替える（失敗時に半端な状態を残さない）
	SpriteAnimationClip loaded;
	try
	{
		const std::vector<SpriteAnimationKey> keys = json.at(kKeyKeys).get<std::vector<SpriteAnimationKey>>();
		// 手書きの JSON は並びが保証されないので、AddKey で時刻順に入れ直す
		for (const SpriteAnimationKey& key : keys)
		{
			loaded.AddKey(key.time, key.texturePath);
		}
		loaded.isLooping_ = json.value(kKeyLoop, true);
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}

	*this = std::move(loaded);
	return true;
}

bool SpriteAnimationClip::LoadFromFile(const std::string& path)
{
	std::ifstream file(path);
	if (!file)
	{
		return false;
	}
	const nlohmann::json json = nlohmann::json::parse(file, nullptr, false);
	return !json.is_discarded() && Deserialize(json);
}

bool SpriteAnimationClip::SaveToFile(const std::string& path) const
{
	const std::filesystem::path filePath(path);
	std::error_code error;
	std::filesystem::create_directories(filePath.parent_path(), error);
	std::ofstream file(filePath);
	if (!file)
	{
		return false;
	}
	file << Serialize().dump(kJsonIndent);
	return true;
}

std::string SpriteAnimationClip::BuildFilePath(const std::string& name)
{
	const std::filesystem::path dir = PathManager::GetApplicationResourceRoot() / "json" / kSaveDirectory;
	return (dir / (name + ".json")).string();
}
} // namespace KCE
