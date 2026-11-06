#include "manager/graphics/ResourceManifest.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "base/Logger.h"
#include "base/DirectXCommon.h"
#include "base/PathManager.h"
#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"
#include "manager/graphics/ModelManager.h"
#include "time/TimeManager.h"
#include "manager/graphics/TextureManager.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
constexpr const char* kFilePath = "json/resources.json";
constexpr int kVersion = 1;
constexpr const char* kLifetimeResident = "Resident";
constexpr const char* kLifetimeScene = "Scene";
// エンジンが前提にしているモデル。表から消すと、起動時にログで知らせる
constexpr const char* kRequiredModels[] = { "cube", "plane", "skydome" };

const char* LifetimeToString(ResourceLifetime lifetime)
{
	return lifetime == ResourceLifetime::Resident ? kLifetimeResident : kLifetimeScene;
}

ResourceLifetime LifetimeFromJson(const nlohmann::json& entry)
{
	if (entry.contains("lifetime") && entry["lifetime"].is_string() && entry["lifetime"].get_ref<const std::string&>() == kLifetimeScene)
	{
		return ResourceLifetime::Scene;
	}
	return ResourceLifetime::Resident;
}

std::string ToLower(std::string text)
{
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}
} // namespace

std::unique_ptr<ResourceManifest> ResourceManifest::instance_ = nullptr;

ResourceManifest* ResourceManifest::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<ResourceManifest>();
	}
	return instance_.get();
}

bool ResourceManifest::HasInstance()
{
	return instance_ != nullptr;
}

std::string ResourceManifest::GetFilePath()
{
	return (PathManager::GetApplicationResourceRoot() / kFilePath).string();
}

void ResourceManifest::Initialize()
{
	Load();
#ifdef USE_IMGUI
	DebugUIManager::GetInstance()->RegisterWindow(this, "Project###Project", [this]() { DrawWindowImGui(); }, EditorDock::Bottom);
#endif
}

void ResourceManifest::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
#endif
	instance_.reset();
}

bool ResourceManifest::Load()
{
	models_.clear();
	textures_.clear();
	dirty_ = false;
	std::ifstream input(GetFilePath());
	if (!input)
	{
		return false;
	}
	try
	{
		nlohmann::json json;
		input >> json;
		if (!json.is_object())
		{
			return false;
		}
		if (json.contains("models") && json["models"].is_array())
		{
			for (const auto& entry : json["models"])
			{
				if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string()) { continue; }
				ModelEntry model;
				model.name = entry["name"].get<std::string>();
				if (entry.contains("type") && entry["type"].is_string()) { model.type = entry["type"].get<std::string>(); }
				model.lifetime = LifetimeFromJson(entry);
				if (!model.name.empty()) { models_.push_back(std::move(model)); }
			}
		}
		if (json.contains("textures") && json["textures"].is_array())
		{
			for (const auto& entry : json["textures"])
			{
				if (!entry.is_object() || !entry.contains("path") || !entry["path"].is_string()) { continue; }
				TextureEntry texture;
				texture.path = entry["path"].get<std::string>();
				texture.lifetime = LifetimeFromJson(entry);
				if (entry.contains("linear") && entry["linear"].is_boolean()) { texture.linear = entry["linear"].get<bool>(); }
				if (!texture.path.empty()) { textures_.push_back(std::move(texture)); }
			}
		}
	}
	catch (const nlohmann::json::exception& error)
	{
		Logger::Log(std::string("ResourceManifest: 読み込み表を読めません: ") + error.what() + "\n", Logger::LogLevel::Warning);
		models_.clear();
		textures_.clear();
		return false;
	}
	return true;
}

bool ResourceManifest::Save()
{
	nlohmann::ordered_json json;
	json["version"] = kVersion;
	json["models"] = nlohmann::ordered_json::array();
	for (const ModelEntry& model : models_)
	{
		json["models"].push_back({ { "name", model.name }, { "type", model.type }, { "lifetime", LifetimeToString(model.lifetime) } });
	}
	json["textures"] = nlohmann::ordered_json::array();
	for (const TextureEntry& texture : textures_)
	{
		json["textures"].push_back({ { "path", texture.path }, { "lifetime", LifetimeToString(texture.lifetime) }, { "linear", texture.linear } });
	}
	const std::filesystem::path fullPath = GetFilePath();
	std::error_code error;
	std::filesystem::create_directories(fullPath.parent_path(), error);
	const std::filesystem::path tempPath = fullPath.string() + ".tmp";
	{
		std::ofstream output(tempPath);
		if (!output) { return false; }
		output << json.dump(4);
		if (!output) { return false; }
	}
	std::filesystem::rename(tempPath, fullPath, error);
	if (error)
	{
		std::filesystem::remove(fullPath, error);
		std::filesystem::rename(tempPath, fullPath, error);
		if (error) { return false; }
	}
	dirty_ = false;
	return true;
}

void ResourceManifest::LoadAll(JobSystem* jobSystem)
{
	// 寿命ごとに、まとめて読む（展開はワーカーで並べて回す）
	for (const ResourceLifetime lifetime : { ResourceLifetime::Resident, ResourceLifetime::Scene })
	{
		std::vector<std::string> texturePaths;
		for (const TextureEntry& texture : textures_)
		{
			if (texture.lifetime != lifetime) { continue; }
			if (texture.linear) { TextureManager::GetInstance()->LoadTextureLinear(texture.path, lifetime); }
			else { texturePaths.push_back(texture.path); }
		}
		if (!texturePaths.empty())
		{
			TextureManager::GetInstance()->LoadTextures(texturePaths, lifetime, jobSystem);
		}
		std::vector<ModelManager::ModelRequest> modelRequests;
		for (const ModelEntry& model : models_)
		{
			if (model.lifetime == lifetime) { modelRequests.push_back({ model.name, model.type }); }
		}
		if (!modelRequests.empty())
		{
			ModelManager::GetInstance()->LoadModels(modelRequests, lifetime, jobSystem);
		}
	}
	// エンジンが必ず使う物が表から消えていたら、起動時に分かるようにする（消した直後の次の起動で気づける）
	for (const char* required : kRequiredModels)
	{
		if (!ModelManager::GetInstance()->HasModel(required))
		{
			Logger::Log(std::string("ResourceManifest: エンジンが使うモデルが読み込まれていません。resources.json を確認してください: ") + required + "\n", Logger::LogLevel::Error);
		}
	}
}

bool ResourceManifest::AddModel(const ModelEntry& entry)
{
	if (entry.name.empty()) { return false; }
	ModelManager::GetInstance()->LoadModel(entry.name, entry.type, entry.lifetime);
	if (!ModelManager::GetInstance()->HasModel(entry.name)) { return false; }
	const auto found = std::find_if(models_.begin(), models_.end(), [&entry](const ModelEntry& m) { return m.name == entry.name; });
	if (found == models_.end()) { models_.push_back(entry); }
	else { *found = entry; }
	dirty_ = true;
	return true;
}

bool ResourceManifest::AddTexture(const TextureEntry& entry)
{
	if (entry.path.empty()) { return false; }
	if (entry.linear) { TextureManager::GetInstance()->LoadTextureLinear(entry.path, entry.lifetime); }
	else { TextureManager::GetInstance()->LoadTexture(entry.path, entry.lifetime); }
	// 読めたかは、読み込み済みの一覧に出ているかで見る
	const auto loaded = TextureManager::GetInstance()->GetLoadedTextures();
	const std::string wantedSuffix = ToLower(std::filesystem::path(entry.path).filename().string());
	const bool ok = std::any_of(loaded.begin(), loaded.end(), [&wantedSuffix](const TextureManager::LoadedTexture& t)
	{
		return ToLower(t.path).size() >= wantedSuffix.size() && ToLower(t.path).find(wantedSuffix) != std::string::npos;
	});
	if (!ok) { return false; }
	const auto found = std::find_if(textures_.begin(), textures_.end(), [&entry](const TextureEntry& t) { return t.path == entry.path && t.linear == entry.linear; });
	if (found == textures_.end()) { textures_.push_back(entry); }
	else { *found = entry; }
	dirty_ = true;
	return true;
}

void ResourceManifest::RequestRelease(bool isModel, const std::string& key)
{
	if (key.empty()) { return; }
	// プレビューが、解放する物を指したままにならないよう、先に外す
	if (!isModel && selectedTexture_ == key) { selectedTexture_.clear(); }
	pendingReleases_.push_back({ isModel, key });
}

void ResourceManifest::ProcessPendingReleases(DirectXCommon* dxCommon)
{
	if (pendingReleases_.empty()) { return; }
	auto requests = std::move(pendingReleases_);
	pendingReleases_.clear();
	// 前のフレームまでの描画が GPU で終わってから外す。描いている最中の物を壊さない
	if (dxCommon) { dxCommon->ExecuteAndWait(); }
	for (const PendingRelease& request : requests)
	{
		if (request.isModel)
		{
			if (ModelManager::GetInstance()->ReleaseModel(request.key))
			{
				RemoveModel(request.key);
				statusMessage_ = "モデルを解放しました: " + request.key;
			}
		}
		else if (TextureManager::GetInstance()->ReleaseTexture(request.key))
		{
			// 表は、ファイル名で見つける（登録名は、表のパスを正規化した形のため）
			const std::string key = request.key;
			const auto entry = std::find_if(textures_.begin(), textures_.end(), [&key](const TextureEntry& t)
			{
				return key.find(std::filesystem::path(t.path).filename().generic_string()) != std::string::npos;
			});
			if (entry != textures_.end()) { RemoveTexture(entry->path); }
			statusMessage_ = "テクスチャを解放しました: " + request.key;
		}
	}
}

void ResourceManifest::RemoveModel(const std::string& name)
{
	const auto before = models_.size();
	models_.erase(std::remove_if(models_.begin(), models_.end(), [&name](const ModelEntry& m) { return m.name == name; }), models_.end());
	dirty_ = dirty_ || models_.size() != before;
}

void ResourceManifest::RemoveTexture(const std::string& path)
{
	const auto before = textures_.size();
	textures_.erase(std::remove_if(textures_.begin(), textures_.end(), [&path](const TextureEntry& t) { return t.path == path; }), textures_.end());
	dirty_ = dirty_ || textures_.size() != before;
}

#ifdef USE_IMGUI
namespace
{
const char* LifetimeLabel(ResourceLifetime lifetime)
{
	return lifetime == ResourceLifetime::Resident ? "常駐" : "シーン";
}

bool PassesFilter(const std::string& text, const char* filter)
{
	return filter[0] == '\0' || ToLower(text).find(ToLower(filter)) != std::string::npos;
}
} // namespace

std::vector<ResourceManifest::ModelEntry> ResourceManifest::ScanModelFiles()
{
	std::vector<ModelEntry> found;
	const std::filesystem::path root = PathManager::GetApplicationResourceRoot() / "models";
	std::error_code error;
	if (!std::filesystem::is_directory(root, error)) { return found; }
	static const std::unordered_set<std::string> kExtensions = { ".obj", ".gltf", ".glb", ".fbx" };
	for (const auto& folder : std::filesystem::directory_iterator(root, error))
	{
		if (!folder.is_directory()) { continue; }
		for (const auto& file : std::filesystem::directory_iterator(folder.path(), error))
		{
			const std::string extension = ToLower(file.path().extension().string());
			if (file.is_regular_file() && kExtensions.contains(extension))
			{
				found.push_back({ folder.path().filename().string(), extension, ResourceLifetime::Resident });
				break;
			}
		}
	}
	std::sort(found.begin(), found.end(), [](const ModelEntry& a, const ModelEntry& b) { return a.name < b.name; });
	return found;
}

std::vector<std::string> ResourceManifest::ScanTextureFiles()
{
	std::vector<std::string> found;
	const std::filesystem::path root = PathManager::GetApplicationResourceRoot();
	const std::filesystem::path textureRoot = root / "textures";
	std::error_code error;
	if (!std::filesystem::is_directory(textureRoot, error)) { return found; }
	static const std::unordered_set<std::string> kExtensions = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
	// textures の中は、これまでの書き方（"./Resources/ファイル名"）に合わせる
	for (const auto& file : std::filesystem::recursive_directory_iterator(textureRoot, error))
	{
		if (file.is_regular_file() && kExtensions.contains(ToLower(file.path().extension().string())))
		{
			found.push_back("./Resources/" + std::filesystem::relative(file.path(), textureRoot).generic_string());
		}
	}
	std::sort(found.begin(), found.end());
	return found;
}

namespace
{
// 「追加」の領域に残す高さ（行の数）。一覧の表は、残りの高さを全部使う
constexpr float kAddAreaLines = 9.0f;
// テクスチャの下の領域（読み込み＋プレビュー）に残す高さ（行の数）と、そのときの最小の幅
constexpr float kBottomAreaLines = 11.0f;
constexpr float kMinPreviewWidth = 200.0f;
constexpr float kMinTableWidth = 240.0f;

/** @brief 最後に参照されたフレームを「N フレーム前」の文字にする。一度も無ければ「—」 */
std::string AgoText(uint64_t lastFrame, uint64_t currentFrame)
{
	if (lastFrame == 0) { return "—"; }
	const uint64_t ago = currentFrame >= lastFrame ? currentFrame - lastFrame : 0;
	return ago == 0 ? "今のフレーム" : std::to_string(ago) + " フレーム前";
}

/** @brief 参照が一度も無い行を、薄い文字にする。PopStyleColor は呼び出し側 */
void BeginUnusedStyle(bool unused)
{
	if (unused) { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); }
}
void EndUnusedStyle(bool unused)
{
	if (unused) { ImGui::PopStyleColor(); }
}

/** @brief 物体（と、その子）のうち、このモデルを使っている数 */
size_t CountObjectsUsingModel(const std::string& modelName)
{
	if (!GameObjectManager::HasInstance()) { return 0; }
	size_t count = 0;
	const auto visit = [&](const auto& self, const GameObject* object) -> void
	{
		if (object->GetModelName() == modelName) { ++count; }
		for (const auto& [childName, child] : object->GetChildren())
		{
			if (child) { self(self, child.get()); }
		}
	};
	for (const GameObject* object : GameObjectManager::GetInstance()->GetGameObjects())
	{
		if (object) { visit(visit, object); }
	}
	return count;
}
} // namespace

void ResourceManifest::DrawWindowImGui()
{
	if (ImGui::Button(dirty_ ? "読み込み表を保存 *" : "読み込み表を保存"))
	{
		statusMessage_ = Save() ? "保存しました" : "保存に失敗しました";
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s", !statusMessage_.empty() ? statusMessage_.c_str()
		: (dirty_ ? "次の起動で読む物が変わっています" : "保存した物は、次の起動で最初に読み込まれます"));

	const float checkWidth = ImGui::CalcTextSize("未使用だけ").x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;
	ImGui::SetNextItemWidth(-checkWidth - ImGui::GetStyle().ItemSpacing.x);
	ImGui::InputTextWithHint("##ResourceFilter", "検索", filter_.data(), filter_.size());
	ImGui::SameLine();
	ImGui::Checkbox("未使用だけ", &unusedOnly_);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("起動後に一度も参照されていない物だけを並べる。\n今の起動で通ったシーンの中だけの結果なので、消す前に、使うシーンを一通り通してから見る");
	}

	if (ImGui::BeginTabBar("##ResourceTabs"))
	{
		if (ImGui::BeginTabItem("モデル")) { DrawModelsTab(); ImGui::EndTabItem(); }
		if (ImGui::BeginTabItem("テクスチャ")) { DrawTexturesTab(); ImGui::EndTabItem(); }
		ImGui::EndTabBar();
	}

	// 解放の確認。メニューの中では開けないので、フラグを受けてここで開く
	if (releasePopupRequested_)
	{
		releasePopupRequested_ = false;
		ImGui::OpenPopup("ReleaseResource");
	}
	if (ImGui::BeginPopupModal("ReleaseResource", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("「%s」を解放します。", releaseConfirmKey_.c_str());
		ImGui::TextDisabled("読み込み表からも外れます（次の起動でも、読み込まれません）。");
		ImGui::TextDisabled("「未参照」は、今回の起動で通った範囲だけの結果です。\n通っていないシーンで使う物なら、消さないでください。");
		if (ImGui::Button("解放する"))
		{
			RequestRelease(releaseConfirmIsModel_, releaseConfirmKey_);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル")) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
}

void ResourceManifest::DrawModelsTab()
{
	static const char* const kLifetimeItems[] = { "常駐", "シーン" };
	const auto loaded = ModelManager::GetInstance()->GetLoadedModels();
	const uint64_t frame = TimeManager::GetInstance().GetFrameCount();
	const size_t unusedCount = static_cast<size_t>(std::count_if(loaded.begin(), loaded.end(), [](const auto& m) { return m.referenceCount == 0; }));
	ImGui::TextDisabled("%zu 件（起動後に未参照: %zu）", loaded.size(), unusedCount);

	std::string removeName;
	const float tableHeight = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * kAddAreaLines;
	if (ImGui::BeginTable("##Models", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV,
		ImVec2(0.0f, (std::max)(tableHeight, ImGui::GetTextLineHeightWithSpacing() * 4.0f))))
	{
		ImGui::TableSetupColumn("名前");
		ImGui::TableSetupColumn("形式", ImGuiTableColumnFlags_WidthFixed, 52.0f);
		ImGui::TableSetupColumn("寿命", ImGuiTableColumnFlags_WidthFixed, 56.0f);
		ImGui::TableSetupColumn("参照", ImGuiTableColumnFlags_WidthFixed, 56.0f);
		ImGui::TableSetupColumn("最後の参照", ImGuiTableColumnFlags_WidthFixed, 110.0f);
		ImGui::TableSetupColumn("使用中", ImGuiTableColumnFlags_WidthFixed, 56.0f);
		ImGui::TableSetupColumn("読み込み表", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();
		for (const auto& model : loaded)
		{
			const bool unused = model.referenceCount == 0;
			if ((unusedOnly_ && !unused) || !PassesFilter(model.name, filter_.data())) { continue; }
			const bool inManifest = std::any_of(models_.begin(), models_.end(), [&model](const ModelEntry& m) { return m.name == model.name; });
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			const size_t usingObjects = CountObjectsUsingModel(model.name);
			ImGui::PushID(model.name.c_str());
			BeginUnusedStyle(unused);
			ImGui::Selectable(model.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
			// メニューは、行の薄い色を引き継がないよう、色を戻してから開く
			EndUnusedStyle(unused);
			if (ImGui::BeginPopupContextItem("ModelRow"))
			{
				ImGui::SeparatorText(model.name.c_str());
				if (ImGui::MenuItem("読み込み表から外す（次の起動から）", nullptr, false, inManifest)) { removeName = model.name; }
				// 使われていない物だけ、今すぐ解放できる
				if (ImGui::MenuItem("解放する…", nullptr, false, unused && usingObjects == 0))
				{
					releaseConfirmIsModel_ = true;
					releaseConfirmKey_ = model.name;
					releasePopupRequested_ = true;
				}
				if (!inManifest) { ImGui::Text("※ 読み込み表に載っていないので、外す操作はできません"); }
				if (!unused || usingObjects != 0) { ImGui::Text("※ 使われているので、解放できません"); }
				ImGui::EndPopup();
			}
			ImGui::PopID();
			BeginUnusedStyle(unused);
			ImGui::TableNextColumn(); ImGui::TextUnformatted(model.type.c_str());
			ImGui::TableNextColumn(); ImGui::TextUnformatted(LifetimeLabel(model.lifetime));
			ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(model.referenceCount));
			ImGui::TableNextColumn(); ImGui::TextUnformatted(AgoText(model.lastReferenceFrame, frame).c_str());
			ImGui::TableNextColumn(); ImGui::Text("%zu", usingObjects);
			ImGui::TableNextColumn(); ImGui::TextUnformatted(inManifest ? "○" : "—");
			EndUnusedStyle(unused);
		}
		ImGui::EndTable();
	}
	if (!removeName.empty()) { RemoveModel(removeName); }

	ImGui::SeparatorText("フォルダから読み込む");
	ImGui::SetNextItemWidth(100.0f);
	ImGui::Combo("寿命##AddModel", &addLifetime_, kLifetimeItems, IM_ARRAYSIZE(kLifetimeItems));
	if (ImGui::BeginChild("##AddableModels", ImVec2(0.0f, 0.0f), true))
	{
		bool any = false;
		for (const ModelEntry& candidate : ScanModelFiles())
		{
			if (ModelManager::GetInstance()->HasModel(candidate.name) || !PassesFilter(candidate.name, filter_.data())) { continue; }
			any = true;
			const std::string label = candidate.name + "  (" + candidate.type + ")";
			if (ImGui::Selectable(label.c_str()))
			{
				ModelEntry entry = candidate;
				entry.lifetime = addLifetime_ == 0 ? ResourceLifetime::Resident : ResourceLifetime::Scene;
				statusMessage_ = AddModel(entry) ? "モデルを読み込みました: " + entry.name : "読み込めませんでした: " + entry.name;
			}
		}
		if (!any) { ImGui::TextDisabled("読み込める物は、ほかにありません。"); }
	}
	ImGui::EndChild();
}

void ResourceManifest::DrawTexturesTab()
{
	static const char* const kLifetimeItems[] = { "常駐", "シーン" };
	const auto loaded = TextureManager::GetInstance()->GetLoadedTextures();
	const uint64_t frame = TimeManager::GetInstance().GetFrameCount();
	const size_t unusedCount = static_cast<size_t>(std::count_if(loaded.begin(), loaded.end(), [](const auto& t) { return t.referenceCount == 0; }));
	ImGui::TextDisabled("%zu 件（起動後に未参照: %zu）", loaded.size(), unusedCount);

	std::string removePath;
	// 表は幅いっぱいに使う（パスが潰れないように）。下に、読み込みの欄とプレビューを横に並べる
	const float tableHeight = (std::max)(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * kBottomAreaLines,
		ImGui::GetTextLineHeightWithSpacing() * 4.0f);
	if (ImGui::BeginTable("##Textures", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV,
		ImVec2(0.0f, tableHeight)))
	{
		ImGui::TableSetupColumn("パス");
		ImGui::TableSetupColumn("大きさ", ImGuiTableColumnFlags_WidthFixed, 88.0f);
		ImGui::TableSetupColumn("寿命", ImGuiTableColumnFlags_WidthFixed, 56.0f);
		ImGui::TableSetupColumn("参照", ImGuiTableColumnFlags_WidthFixed, 56.0f);
		ImGui::TableSetupColumn("最後の参照", ImGuiTableColumnFlags_WidthFixed, 110.0f);
		ImGui::TableSetupColumn("読み込み表", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();
		for (const auto& texture : loaded)
		{
			const bool unused = texture.referenceCount == 0;
			if ((unusedOnly_ && !unused) || !PassesFilter(texture.path, filter_.data())) { continue; }
			const auto manifestEntry = std::find_if(textures_.begin(), textures_.end(), [&texture](const TextureEntry& t)
			{
				return texture.path.find(std::filesystem::path(t.path).filename().generic_string()) != std::string::npos;
			});
			const bool inManifest = manifestEntry != textures_.end();
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(texture.path.c_str());
			BeginUnusedStyle(unused);
			if (ImGui::Selectable(texture.path.c_str(), selectedTexture_ == texture.path, ImGuiSelectableFlags_SpanAllColumns))
			{
				selectedTexture_ = texture.path;
			}
			// メニューは、行の薄い色を引き継がないよう、色を戻してから開く
			EndUnusedStyle(unused);
			if (ImGui::BeginPopupContextItem("TextureRow"))
			{
				ImGui::SeparatorText(texture.path.c_str());
				if (ImGui::MenuItem("読み込み表から外す（次の起動から）", nullptr, false, inManifest)) { removePath = manifestEntry->path; }
				// 使われていない物だけ、今すぐ解放できる
				if (ImGui::MenuItem("解放する…", nullptr, false, unused))
				{
					releaseConfirmIsModel_ = false;
					releaseConfirmKey_ = texture.path;
					releasePopupRequested_ = true;
				}
				if (!inManifest) { ImGui::Text("※ 読み込み表に載っていないので、外す操作はできません"); }
				if (!unused) { ImGui::Text("※ 使われているので、解放できません"); }
				ImGui::EndPopup();
			}
			ImGui::PopID();
			BeginUnusedStyle(unused);
			ImGui::TableNextColumn(); ImGui::Text("%u x %u", texture.width, texture.height);
			ImGui::TableNextColumn(); ImGui::TextUnformatted(LifetimeLabel(texture.lifetime));
			ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(texture.referenceCount));
			ImGui::TableNextColumn(); ImGui::TextUnformatted(AgoText(texture.lastReferenceFrame, frame).c_str());
			ImGui::TableNextColumn(); ImGui::TextUnformatted(inManifest ? "○" : "—");
			EndUnusedStyle(unused);
		}
		ImGui::EndTable();
	}
	if (!removePath.empty()) { RemoveTexture(removePath); }

	// 下の左: フォルダから読み込む。下の右: 選んだテクスチャのプレビュー
	const float bottomWidth = ImGui::GetContentRegionAvail().x;
	const float previewWidth = (std::max)(bottomWidth * 0.4f, kMinPreviewWidth);
	const float loadWidth = (std::max)(bottomWidth - previewWidth - ImGui::GetStyle().ItemSpacing.x, kMinTableWidth);
	if (ImGui::BeginChild("##LoadTextures", ImVec2(loadWidth, 0.0f), true))
	{
		ImGui::TextUnformatted("フォルダから読み込む");
		ImGui::SetNextItemWidth(90.0f);
		ImGui::Combo("寿命##AddTexture", &addLifetime_, kLifetimeItems, IM_ARRAYSIZE(kLifetimeItems));
		ImGui::SameLine();
		ImGui::Checkbox("リニアで読む", &addLinear_);
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("法線マップなど、色ではなく数値として使う画像"); }
		if (ImGui::BeginChild("##AddableTextures", ImVec2(0.0f, 0.0f), false))
		{
			bool any = false;
			for (const std::string& candidate : ScanTextureFiles())
			{
				const std::string fileName = std::filesystem::path(candidate).filename().generic_string();
				const bool isLoaded = std::any_of(loaded.begin(), loaded.end(), [&fileName](const auto& t) { return t.path.find(fileName) != std::string::npos; });
				if (isLoaded || !PassesFilter(candidate, filter_.data())) { continue; }
				any = true;
				if (ImGui::Selectable(candidate.c_str()))
				{
					TextureEntry entry;
					entry.path = candidate;
					entry.lifetime = addLifetime_ == 0 ? ResourceLifetime::Resident : ResourceLifetime::Scene;
					entry.linear = addLinear_;
					statusMessage_ = AddTexture(entry) ? "テクスチャを読み込みました: " + candidate : "読み込めませんでした: " + candidate;
				}
			}
			if (!any) { ImGui::TextDisabled("読み込める物は、ほかにありません。"); }
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();

	ImGui::SameLine();
	if (ImGui::BeginChild("##TexturePreview", ImVec2(0.0f, 0.0f), true))
	{
		const auto selected = std::find_if(loaded.begin(), loaded.end(), [this](const auto& t) { return t.path == selectedTexture_; });
		if (selected == loaded.end())
		{
			ImGui::TextDisabled("一覧から選ぶと、ここにプレビューが出ます。");
		}
		else
		{
			ImGui::TextDisabled("%u x %u", selected->width, selected->height);
			if (selected->previewable && selected->gpuHandle != 0 && selected->width > 0 && selected->height > 0)
			{
				// 縦横比を保って、残りの欄に収まる大きさで出す
				const ImVec2 avail = ImGui::GetContentRegionAvail();
				const float scale = (std::min)(avail.x / static_cast<float>(selected->width), avail.y / static_cast<float>(selected->height));
				ImGui::Image(static_cast<ImTextureID>(selected->gpuHandle),
					ImVec2(static_cast<float>(selected->width) * scale, static_cast<float>(selected->height) * scale));
			}
			else
			{
				ImGui::TextDisabled("この形式（キューブマップなど）は、プレビューできません。");
			}
		}
	}
	ImGui::EndChild();
}
#endif
} // namespace KCE
