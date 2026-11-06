#include "manager/editor/SettingsStore.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "base/Logger.h"
#include "base/PathManager.h"

namespace KCE
{
namespace
{
constexpr const char* kFileName = "editor_settings.json";
constexpr const char* kDirectory = "json/settings";
constexpr const char* kVersionKey = "version";
constexpr int kVersion = 1;
} // namespace

std::unique_ptr<SettingsStore> SettingsStore::instance_ = nullptr;

SettingsStore* SettingsStore::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<SettingsStore>();
	}
	return instance_.get();
}

bool SettingsStore::HasInstance()
{
	return instance_ != nullptr;
}

std::string SettingsStore::GetFilePath()
{
	return (PathManager::GetApplicationResourceRoot() / kDirectory / kFileName).string();
}

void SettingsStore::Initialize()
{
	loaded_ = nlohmann::ordered_json::object();
	std::ifstream input(GetFilePath());
	if (!input)
	{
		return;
	}
	try
	{
		nlohmann::json json;
		input >> json;
		if (json.is_object())
		{
			loaded_ = nlohmann::ordered_json(json);
		}
	}
	catch (const nlohmann::json::exception& error)
	{
		Logger::Log(std::string("SettingsStore: 設定ファイルを読めません: ") + error.what() + "\n", Logger::LogLevel::Warning);
	}
}

void SettingsStore::Finalize()
{
	items_.clear();
	instance_.reset();
}

void SettingsStore::RegisterItem(void* owner, const std::string& key,
	std::function<nlohmann::json()> serialize, std::function<void(const nlohmann::json&)> deserialize)
{
	if (!owner || key.empty() || !serialize || !deserialize)
	{
		return;
	}
	const auto existing = std::find_if(items_.begin(), items_.end(), [&key](const Item& item) { return item.key == key; });
	if (existing != items_.end())
	{
		Logger::Log("SettingsStore: 同じキーを二重に登録しました: " + key + "\n", Logger::LogLevel::Warning);
		return;
	}
	// ファイルに値があれば、その場で戻す。形が違って例外になっても、既定値のまま続ける
	if (loaded_.contains(key))
	{
		try
		{
			deserialize(nlohmann::json(loaded_[key]));
		}
		catch (const nlohmann::json::exception& error)
		{
			Logger::Log("SettingsStore: " + key + " の値を戻せません: " + error.what() + "\n", Logger::LogLevel::Warning);
		}
	}
	items_.push_back({ owner, key, std::move(serialize), std::move(deserialize) });
	// 登録のたびに、保存済みとの比較の基準を取り直す（復元した直後は、保存済みと同じ扱い）
	savedSnapshot_[key] = items_.back().serialize();
}

void SettingsStore::Unregister(void* owner)
{
	items_.erase(std::remove_if(items_.begin(), items_.end(), [owner](const Item& item) { return item.owner == owner; }), items_.end());
}

nlohmann::ordered_json SettingsStore::Collect() const
{
	nlohmann::ordered_json json = loaded_;
	json[kVersionKey] = kVersion;
	for (const Item& item : items_)
	{
		json[item.key] = item.serialize();
	}
	return json;
}

bool SettingsStore::Save()
{
	const std::filesystem::path fullPath = GetFilePath();
	std::error_code error;
	std::filesystem::create_directories(fullPath.parent_path(), error);
	const std::filesystem::path tempPath = fullPath.string() + ".tmp";
	const nlohmann::ordered_json json = Collect();
	{
		std::ofstream output(tempPath);
		if (!output)
		{
			statusMessage_ = "保存先を開けませんでした";
			return false;
		}
		output << json.dump(4);
		if (!output)
		{
			statusMessage_ = "書き込みに失敗しました";
			return false;
		}
	}
	std::filesystem::rename(tempPath, fullPath, error);
	if (error)
	{
		std::filesystem::remove(fullPath, error);
		std::filesystem::rename(tempPath, fullPath, error);
		if (error)
		{
			statusMessage_ = "保存の確定に失敗しました";
			return false;
		}
	}
	loaded_ = json;
	savedSnapshot_ = json;
	statusMessage_ = "設定を保存しました";
	return true;
}

bool SettingsStore::Reload()
{
	Initialize();
	for (const Item& item : items_)
	{
		if (!loaded_.contains(item.key))
		{
			continue;
		}
		try
		{
			item.deserialize(nlohmann::json(loaded_[item.key]));
		}
		catch (const nlohmann::json::exception& error)
		{
			Logger::Log("SettingsStore: " + item.key + " の値を戻せません: " + error.what() + "\n", Logger::LogLevel::Warning);
		}
	}
	savedSnapshot_ = Collect();
	statusMessage_ = "保存した設定を読み込みました";
	return true;
}

bool SettingsStore::IsDirty() const
{
	// 登録された項目の値だけを比べる（version や、未登録のキーは対象外）
	for (const Item& item : items_)
	{
		const auto saved = savedSnapshot_.find(item.key);
		if (saved == savedSnapshot_.end() || *saved != nlohmann::ordered_json(item.serialize()))
		{
			return true;
		}
	}
	return false;
}
} // namespace KCE
