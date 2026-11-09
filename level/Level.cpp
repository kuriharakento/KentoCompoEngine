#include "level/Level.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "base/Logger.h"
#include "base/PathManager.h"
#include "core/SchemaVersion.h"
#include "scene/SceneContext.h"
#ifdef USE_IMGUI
#include "editor/SelectionContext.h"
#endif
#include "level/LevelManager.h"

namespace KCE
{
namespace
{
constexpr const char* kLevelDirectory = "json/level";
constexpr const char* kVersionKey = "version";
} // namespace

std::string ILevelElement::MakeObjectName(const std::string& elementName) const
{
	return level_ ? "Level_" + level_->GetName() + "_" + elementName : elementName;
}

Level::Level(std::string name, const SceneContext& context)
	: name_(std::move(name)), context_(context)
{
}

Level::~Level()
{
	ClearElements();
}

void Level::ClearElements()
{
	// 後ろに足された物（モニターなど、前の物を参照する側）から先に壊す。
	// 映す側が、映される側（カメラの映像）より先に畳まれるようにする
	while (!elements_.empty())
	{
		elements_.pop_back();
	}
}

bool Level::Load()
{
	ClearElements();
	passthrough_ = nlohmann::ordered_json::object();

	const std::filesystem::path fullPath = PathManager::GetApplicationResourceRoot() / kLevelDirectory / (name_ + ".json");
	if (!std::filesystem::exists(fullPath))
	{
		savedSnapshot_ = Serialize();
		Logger::Log("Level: 新しい空の Level を開きました: " + name_ + "\n", Logger::LogLevel::Info);
		return true;
	}
	std::ifstream input(fullPath);
	if (!input)
	{
		Logger::Log("Level: ファイルを開けません: " + fullPath.string() + "\n", Logger::LogLevel::Error);
		return false;
	}
	try
	{
		nlohmann::json json;
		input >> json;
		std::string error;
		if (!Deserialize(json, error))
		{
			Logger::Log("Level: 読み込みに失敗しました: " + error + "\n", Logger::LogLevel::Error);
			ClearElements();
			passthrough_ = nlohmann::ordered_json::object();
			return false;
		}
	}
	catch (const nlohmann::json::exception& e)
	{
		Logger::Log(std::string("Level: JSON を読めません: ") + e.what() + "\n", Logger::LogLevel::Error);
		ClearElements();
		passthrough_ = nlohmann::ordered_json::object();
		return false;
	}
	savedSnapshot_ = Serialize();
	return true;
}

bool Level::Save()
{
	const std::filesystem::path fullPath = PathManager::GetApplicationResourceRoot() / kLevelDirectory / (name_ + ".json");
	std::error_code error;
	std::filesystem::create_directories(fullPath.parent_path(), error);
	const std::filesystem::path tempPath = fullPath.string() + ".tmp";
	const nlohmann::ordered_json json = Serialize();
	{
		std::ofstream output(tempPath);
		if (!output)
		{
			Logger::Log("Level: 保存先を開けませんでした: " + tempPath.string() + "\n", Logger::LogLevel::Error);
			return false;
		}
		output << json.dump(4);
		if (!output)
		{
			Logger::Log("Level: 書き込みに失敗しました: " + tempPath.string() + "\n", Logger::LogLevel::Error);
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
			Logger::Log("Level: 保存の確定に失敗しました: " + error.message() + "\n", Logger::LogLevel::Error);
			return false;
		}
	}
	savedSnapshot_ = json;
	return true;
}

void Level::Update(float deltaTime)
{
	for (auto& element : elements_)
	{
		element->Update(deltaTime);
	}
}

void Level::SetPlaying(bool playing)
{
	playing_ = playing;
	for (auto& element : elements_)
	{
		element->SetPlaying(playing);
	}
}

ILevelElement* Level::FindElement(const std::string& name) const
{
	for (const auto& element : elements_)
	{
		if (element->GetName() == name)
		{
			return element.get();
		}
	}
	return nullptr;
}

bool Level::Add(std::unique_ptr<ILevelElement>& element)
{
	if (!element || element->GetName().empty() || FindElement(element->GetName()))
	{
		return false;
	}
	// 編集中に足した物は、止まった状態から始める
	element->SetPlaying(playing_);
	element->BindLevel(this);
	elements_.push_back(std::move(element));
	return true;
}

bool Level::Rename(const std::string& oldName, const std::string& newName)
{
	ILevelElement* target = FindElement(oldName);
	if (!target || newName.empty() || FindElement(newName))
	{
		return false;
	}
	if (!target->OnRename(newName))
	{
		return false;
	}
	target->SetName(newName);
	for (auto& element : elements_)
	{
		element->OnElementRenamed(oldName, newName);
	}
#ifdef USE_IMGUI
	// 選んでいたなら、新しい名前の物を選び直す
	const SelectionItem primary = SelectionContext::GetInstance()->GetPrimary();
	if (!primary.name.empty() && primary.name == oldName)
	{
		SelectionItem renamed = primary;
		renamed.name = newName;
		SelectionContext::GetInstance()->Select(renamed);
	}
#endif
	return true;
}

std::unique_ptr<ILevelElement> Level::Remove(const std::string& name)
{
	for (auto it = elements_.begin(); it != elements_.end(); ++it)
	{
		if ((*it)->GetName() == name)
		{
			std::unique_ptr<ILevelElement> removed = std::move(*it);
			elements_.erase(it);
			return removed;
		}
	}
	return nullptr;
}

bool Level::IsDirty() const
{
	return Serialize() != savedSnapshot_;
}

nlohmann::ordered_json Level::Serialize() const
{
	nlohmann::ordered_json json;
	json[kVersionKey] = kStageSchemaVersion;
	// 登録順に種類ごとの配列を書く。読み込みもこの順で行う
	for (const LevelElementType& type : LevelManager::GetInstance()->GetElementTypes())
	{
		nlohmann::ordered_json array = nlohmann::ordered_json::array();
		for (const auto& element : elements_)
		{
			if (type.key == element->GetTypeKey())
			{
				array.push_back(element->Serialize());
			}
		}
		json[type.key] = std::move(array);
	}
	for (auto it = passthrough_.begin(); it != passthrough_.end(); ++it)
	{
		json[it.key()] = it.value();
	}
	return json;
}

bool Level::Deserialize(const nlohmann::json& json, std::string& outError)
{
	if (!json.is_object())
	{
		outError = "ルートがオブジェクトではありません";
		return false;
	}
	if (!json.contains(kVersionKey) || !json[kVersionKey].is_number_integer())
	{
		outError = "version がありません";
		return false;
	}
	const int version = json[kVersionKey].get<int>();
	if (!IsLoadableSchemaVersion(version, kStageSchemaVersion))
	{
		outError = "対応していないスキーマバージョンです (file=" + std::to_string(version) + ", engine=" + std::to_string(kStageSchemaVersion) + ")";
		return false;
	}

	const auto& types = LevelManager::GetInstance()->GetElementTypes();
	const auto isRegisteredKey = [&types](const std::string& key)
	{
		return std::any_of(types.begin(), types.end(), [&key](const LevelElementType& type) { return type.key == key; });
	};
	// 登録されていない項目は、保存で消さないよう取っておく
	for (auto it = json.begin(); it != json.end(); ++it)
	{
		if (it.key() != kVersionKey && !isRegisteredKey(it.key()))
		{
			passthrough_[it.key()] = nlohmann::ordered_json(it.value());
		}
	}

	// 登録順に読む。参照する側の種類は、参照される側より後に登録されている
	for (const LevelElementType& type : types)
	{
		if (!json.contains(type.key))
		{
			continue;
		}
		if (!json[type.key].is_array())
		{
			outError = type.key + " が配列ではありません";
			return false;
		}
		for (const auto& entry : json[type.key])
		{
			std::unique_ptr<ILevelElement> element;
			try
			{
				element = type.create ? type.create(entry, context_) : nullptr;
			}
			catch (const nlohmann::json::exception& e)
			{
				Logger::Log(std::string("Level: ") + type.key + " の1件を読めません: " + e.what() + "\n", Logger::LogLevel::Warning);
			}
			if (!element)
			{
				Logger::Log("Level: 不正な " + type.key + " の項目を読み飛ばしました\n", Logger::LogLevel::Warning);
				continue;
			}
			if (FindElement(element->GetName()))
			{
				Logger::Log("Level: 名前が重複する項目を読み飛ばしました: " + element->GetName() + "\n", Logger::LogLevel::Warning);
				continue;
			}
			// 読み込んだ時点の再生状態に合わせる。要素は動く状態で作られるので、止めておかないと読んだ瞬間から動いてしまう
			element->SetPlaying(playing_);
			element->BindLevel(this);
			elements_.push_back(std::move(element));
		}
	}
	return true;
}
} // namespace KCE
