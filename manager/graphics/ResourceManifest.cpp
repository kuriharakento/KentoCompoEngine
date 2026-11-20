#include "manager/graphics/ResourceManifest.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>
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
	DebugUIManager::GetInstance()->RegisterWindow(this, "Assets###Project", [this]() { DrawWindowImGui(); }, EditorDock::Bottom);
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
	savedTextures_.clear();
	savedModels_.clear();
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
	savedTextures_ = textures_;
	savedModels_ = models_;
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
	savedTextures_ = textures_;
	savedModels_ = models_;
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

void ResourceManifest::LogLoadedResources() const
{
	const auto lifetimeText = [](ResourceLifetime lifetime) { return lifetime == ResourceLifetime::Resident ? "常駐" : "シーン"; };
	const auto models = ModelManager::GetInstance()->GetLoadedModels();
	const auto textures = TextureManager::GetInstance()->GetLoadedTextures();
	std::string text = "===== 読み込み済みリソース =====\n";
	text += "[モデル] " + std::to_string(models.size()) + " 件\n";
	for (const auto& m : models)
	{
		const bool boot = std::any_of(models_.begin(), models_.end(), [&m](const ModelEntry& e) { return e.name == m.name; });
		text += "  " + m.name + "  " + m.type + "  " + lifetimeText(m.lifetime) + (boot ? "  [起動時に読む]" : "") + "\n";
	}
	text += "[テクスチャ] " + std::to_string(textures.size()) + " 件\n";
	for (const auto& t : textures)
	{
		text += "  " + t.path + "  " + std::to_string(t.width) + "x" + std::to_string(t.height) + "  " + lifetimeText(t.lifetime) + "\n";
	}
	text += "================================\n";
	Logger::Log(text);
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

/**
 * @brief 一覧に出すためのテクスチャのパス。みんな同じ先頭（./Resources/textures/）を外す
 * @details 表示だけの変換。読み込みや保存に使うパスは変えない。先頭が無い書き方（"textures/x.png" など）も同じに見せる
 */
std::string DisplayTexturePath(const std::string& path)
{
	std::string text = path;
	for (char& c : text) { if (c == '\\') { c = '/'; } }
	// 先頭の取り除き。大文字小文字は区別しない。上から順に、あれば外す
	static const char* const kPrefixes[] = { "./", "application/", "resources/", "textures/" };
	for (const char* prefix : kPrefixes)
	{
		const size_t length = std::char_traits<char>::length(prefix);
		if (text.size() > length && ToLower(text.substr(0, length)) == prefix)
		{
			text.erase(0, length);
		}
	}
	return text;
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
	// Resources からの相対で書く（"./Resources/textures/ui/x.png"）。サブフォルダの中でも、このパスで読み込める
	for (const auto& file : std::filesystem::recursive_directory_iterator(textureRoot, error))
	{
		if (file.is_regular_file() && kExtensions.contains(ToLower(file.path().extension().string())))
		{
			found.push_back("./Resources/textures/" + std::filesystem::relative(file.path(), textureRoot).generic_string());
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
	if (ImGui::Button("一覧をログへ出力"))
	{
		LogLoadedResources();
		statusMessage_ = "読み込み済みの一覧をログへ出力しました";
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

namespace
{
/** @brief TextureManager が登録名にしている形（./ を解決し、スラッシュ区切り、小文字）に揃える */
std::string NormalizeKey(const std::string& path)
{
	return ToLower(std::filesystem::path(path).lexically_normal().generic_string());
}

/** @brief 表のパスを、ファイルのキーと比べられる形にしたもの（昔の "./Resources/ファイル名" 書きのため、ファイル名も持つ） */
struct ManifestKeys
{
	std::vector<std::string> keys;
	std::vector<std::string> names;
};

template <class List>
ManifestKeys BuildManifestKeys(const List& list)
{
	ManifestKeys result;
	for (const auto& entry : list)
	{
		result.keys.push_back(NormalizeKey(entry.path));
		result.names.push_back(ToLower(std::filesystem::path(entry.path).filename().generic_string()));
	}
	return result;
}

/** @return 表の何番目か。無ければ -1。キーが同じ物を先に、無ければ「どのファイルにも当たらない昔の書き方」のファイル名で探す */
int FindManifestIndex(const ManifestKeys& list, const std::string& key, const std::string& nameLower, const std::unordered_set<std::string>& fileKeys)
{
	for (size_t i = 0; i < list.keys.size(); ++i)
	{
		if (list.keys[i] == key) { return static_cast<int>(i); }
	}
	for (size_t i = 0; i < list.keys.size(); ++i)
	{
		if (!fileKeys.contains(list.keys[i]) && list.names[i] == nameLower) { return static_cast<int>(i); }
	}
	return -1;
}

constexpr ImU32 kColorReferenced = IM_COL32(80, 200, 120, 255);
constexpr ImU32 kColorUnreferenced = IM_COL32(220, 170, 60, 255);
constexpr ImU32 kColorChanged = IM_COL32(255, 140, 40, 255);
} // namespace

void ResourceManifest::RescanAssets()
{
	assetFiles_.clear();
	folderRoot_ = FolderNode{};
	folderRoot_.name = "textures";
	static const std::string kPrefix = "./Resources/textures/";
	for (const std::string& path : ScanTextureFiles())
	{
		AssetFile file;
		file.path = path;
		file.relative = path.substr(kPrefix.size());
		const size_t slash = file.relative.find_last_of('/');
		file.folder = slash == std::string::npos ? std::string() : file.relative.substr(0, slash);
		file.name = slash == std::string::npos ? file.relative : file.relative.substr(slash + 1);
		file.key = NormalizeKey(path);
		assetFiles_.push_back(std::move(file));
	}
	// フォルダの木。ファイルのフォルダを、上から順にたどって作る
	for (const AssetFile& file : assetFiles_)
	{
		FolderNode* node = &folderRoot_;
		size_t start = 0;
		while (start < file.folder.size())
		{
			size_t end = file.folder.find('/', start);
			if (end == std::string::npos) { end = file.folder.size(); }
			const std::string name = file.folder.substr(start, end - start);
			auto child = std::find_if(node->children.begin(), node->children.end(), [&name](const FolderNode& n) { return n.name == name; });
			if (child == node->children.end())
			{
				FolderNode created;
				created.name = name;
				created.folder = file.folder.substr(0, end);
				node->children.push_back(std::move(created));
				child = std::prev(node->children.end());
			}
			node = &*child;
			start = end + 1;
		}
	}
	// 選んでいたフォルダが無くなっていたら、根へ戻す
	if (!selectedFolder_.empty())
	{
		const bool exists = std::any_of(assetFiles_.begin(), assetFiles_.end(), [this](const AssetFile& f)
		{
			return f.folder == selectedFolder_ || f.folder.rfind(selectedFolder_ + "/", 0) == 0;
		});
		if (!exists) { selectedFolder_.clear(); }
	}
	assetsScanned_ = true;
}

void ResourceManifest::DrawFolderTree(const FolderNode& node)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if (node.children.empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }
	if (selectedFolder_ == node.folder) { flags |= ImGuiTreeNodeFlags_Selected; }
	const std::string label = node.name + "###folder:" + node.folder;
	const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { selectedFolder_ = node.folder; }
	if (open)
	{
		for (const FolderNode& child : node.children) { DrawFolderTree(child); }
		ImGui::TreePop();
	}
}

void ResourceManifest::DrawTexturesTab()
{
	using Loaded = TextureManager::LoadedTexture;
	static const char* const kLifetimeItems[] = { "常駐", "シーン" };
	if (!assetsScanned_) { RescanAssets(); }
	const auto loaded = TextureManager::GetInstance()->GetLoadedTextures();
	const uint64_t frame = TimeManager::GetInstance().GetFrameCount();

	// ---- 1 フレームに 1 回、ファイルごとの状態を決める ----
	std::unordered_map<std::string, const Loaded*> loadedByKey;
	for (const Loaded& t : loaded) { loadedByKey[t.path] = &t; }
	std::unordered_set<std::string> fileKeys;
	for (const AssetFile& f : assetFiles_) { fileKeys.insert(f.key); }
	const ManifestKeys bootKeys = BuildManifestKeys(textures_);
	const ManifestKeys savedKeys = BuildManifestKeys(savedTextures_);

	struct Row
	{
		// 読み込み済みなら、その情報。未読み込みなら nullptr
		const Loaded* loaded = nullptr;
		// 起動時に読む表の何番目か。載っていなければ -1
		int bootIndex = -1;
		// 最後に保存した表に載っていたか
		bool saved = false;
	};
	std::vector<Row> rows(assetFiles_.size());
	std::unordered_set<const Loaded*> matched;
	size_t changedCount = 0;
	for (size_t i = 0; i < assetFiles_.size(); ++i)
	{
		const AssetFile& file = assetFiles_[i];
		Row& row = rows[i];
		const std::string nameLower = ToLower(file.name);
		if (const auto it = loadedByKey.find(file.key); it != loadedByKey.end()) { row.loaded = it->second; }
		else if (const auto linear = loadedByKey.find(file.key + "|linear"); linear != loadedByKey.end()) { row.loaded = linear->second; }
		else
		{
			// 昔の書き方（"./Resources/ファイル名"）で読まれた物は、ファイル名の末尾で探す
			const std::string suffix = "/" + nameLower;
			for (const Loaded& t : loaded)
			{
				if (!fileKeys.contains(t.path) && t.path.size() >= suffix.size() && t.path.compare(t.path.size() - suffix.size(), suffix.size(), suffix) == 0)
				{
					row.loaded = &t;
					break;
				}
			}
		}
		if (row.loaded) { matched.insert(row.loaded); }
		row.bootIndex = FindManifestIndex(bootKeys, file.key, nameLower, fileKeys);
		row.saved = FindManifestIndex(savedKeys, file.key, nameLower, fileKeys) >= 0;
		if ((row.bootIndex >= 0) != row.saved) { ++changedCount; }
	}
	const size_t bootCount = static_cast<size_t>(std::count_if(rows.begin(), rows.end(), [](const Row& r) { return r.bootIndex >= 0; }));

	// 起動時に読む / 読まないを切り替える。表を変えるので、以降の bootIndex は使わない
	const auto setBoot = [this](const AssetFile& file, bool boot)
	{
		if (boot)
		{
			TextureEntry entry;
			entry.path = file.path;
			entry.lifetime = ResourceLifetime::Resident;
			statusMessage_ = AddTexture(entry) ? "起動時に読むようにしました: " + file.relative : "読み込めませんでした: " + file.relative;
		}
		else
		{
			const std::string key = file.key;
			const std::string nameLower = ToLower(file.name);
			std::unordered_set<std::string> fileKeysNow;
			for (const AssetFile& f : assetFiles_) { fileKeysNow.insert(f.key); }
			const int index = FindManifestIndex(BuildManifestKeys(textures_), key, nameLower, fileKeysNow);
			if (index >= 0)
			{
				RemoveTexture(textures_[static_cast<size_t>(index)].path);
				statusMessage_ = "起動時に読まないようにしました（保存すると次の起動から）: " + file.relative;
			}
		}
	};
	const auto requestRelease = [this](const Loaded& t)
	{
		releaseConfirmIsModel_ = false;
		releaseConfirmKey_ = t.path;
		releasePopupRequested_ = true;
	};

	// ---- 上の行: 件数と操作 ----
	ImGui::Text("読み込み済み %zu 件 / 起動時に読む %zu 件", loaded.size(), bootCount);
	if (loaded.size() > matched.size())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("（textures の外: %zu 件）", loaded.size() - matched.size());
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("モデルに付いている画像や、フォントの画像など。このフォルダ表示には出ない"); }
	}
	if (changedCount > 0)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColorChanged), "未保存の変更 %zu 件", changedCount);
	}
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat("大きさ##Thumbnail", &thumbnailSize_, 48.0f, 160.0f, "%.0f");
	ImGui::SameLine();
	ImGui::Checkbox("サブフォルダも", &includeSubfolders_);
	ImGui::SameLine();
	if (ImGui::Button("探し直す")) { RescanAssets(); }
	ImGui::SameLine();
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColorReferenced), "●"); ImGui::SameLine(0.0f, 2.0f); ImGui::TextDisabled("参照あり");
	ImGui::SameLine();
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColorUnreferenced), "●"); ImGui::SameLine(0.0f, 2.0f); ImGui::TextDisabled("未参照");
	ImGui::SameLine();
	ImGui::TextDisabled("□ 起動時に読む");
	ImGui::SameLine();
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColorChanged), "枠"); ImGui::SameLine(0.0f, 2.0f); ImGui::TextDisabled("未保存の変更");

	// ---- 本体: 左にフォルダ、右にファイルと詳細 ----
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const float treeWidth = (std::clamp)(avail.x * 0.22f, 120.0f, 260.0f);
	if (ImGui::BeginChild("##AssetFolders", ImVec2(treeWidth, 0.0f), true))
	{
		DrawFolderTree(folderRoot_);
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginGroup();
	const float detailHeight = ImGui::GetTextLineHeightWithSpacing() * 8.5f;
	const float gridHeight = (std::max)(ImGui::GetContentRegionAvail().y - detailHeight - ImGui::GetStyle().ItemSpacing.y, 60.0f);
	if (ImGui::BeginChild("##AssetGrid", ImVec2(0.0f, gridHeight), true))
	{
		const bool filtering = filter_[0] != '\0';
		ImGui::TextDisabled("%s", filtering ? "検索結果（全部のフォルダから）" : (selectedFolder_.empty() ? "textures" : ("textures/" + selectedFolder_).c_str()));
		const float size = thumbnailSize_;
		const ImGuiStyle& style = ImGui::GetStyle();
		const float lineHeight = ImGui::GetTextLineHeight();
		const float checkWidth = ImGui::GetFrameHeight();
		const int columns = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x + style.ItemSpacing.x) / (size + style.ItemSpacing.x)));
		int shown = 0;
		for (size_t i = 0; i < assetFiles_.size(); ++i)
		{
			const AssetFile& file = assetFiles_[i];
			const Row& row = rows[i];
			if (filtering)
			{
				if (!PassesFilter(file.relative, filter_.data())) { continue; }
			}
			else if (includeSubfolders_)
			{
				if (!selectedFolder_.empty() && file.folder != selectedFolder_ && file.folder.rfind(selectedFolder_ + "/", 0) != 0) { continue; }
			}
			else if (file.folder != selectedFolder_) { continue; }
			if (unusedOnly_ && !(row.loaded && row.loaded->referenceCount == 0)) { continue; }

			if (shown % columns != 0) { ImGui::SameLine(); }
			++shown;
			ImGui::PushID(file.path.c_str());
			ImGui::BeginGroup();
			const ImVec2 pos = ImGui::GetCursorScreenPos();
			if (ImGui::Selectable("##thumb", selectedAsset_ == file.path, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(size, size)))
			{
				selectedAsset_ = file.path;
				// ダブルクリックで、読み込まれていなければ試しに読む（読み込み表には載せない。シーンの寿命）
				if (ImGui::IsMouseDoubleClicked(0) && !row.loaded)
				{
					TextureManager::GetInstance()->LoadTexture(file.path, ResourceLifetime::Scene);
				}
			}
			if (ImGui::BeginPopupContextItem("AssetMenu"))
			{
				selectedAsset_ = file.path;
				bool boot = row.bootIndex >= 0;
				if (ImGui::MenuItem("起動時に読む", nullptr, &boot)) { setBoot(file, boot); }
				if (ImGui::MenuItem("今読み込む（読み込み表には載せない）", nullptr, false, row.loaded == nullptr))
				{
					TextureManager::GetInstance()->LoadTexture(file.path, ResourceLifetime::Scene);
				}
				if (ImGui::MenuItem("解放する…", nullptr, false, row.loaded != nullptr && row.loaded->referenceCount == 0)) { requestRelease(*row.loaded); }
				if (row.loaded && row.loaded->referenceCount != 0) { ImGui::TextDisabled("※ 使われているので、解放できません"); }
				ImGui::EndPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s\n%s", file.relative.c_str(), row.loaded ? "読み込み済み" : "未読み込み（ダブルクリックで読む）");
			}

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const ImVec2 inMin(pos.x + 2.0f, pos.y + 2.0f);
			const ImVec2 inMax(pos.x + size - 2.0f, pos.y + size - 2.0f);
			drawList->AddRectFilled(inMin, inMax, IM_COL32(36, 36, 36, 255));
			if (row.loaded && row.loaded->previewable && row.loaded->gpuHandle != 0 && row.loaded->width > 0 && row.loaded->height > 0)
			{
				// 縦横比を保って、枠に収める
				const float boxW = inMax.x - inMin.x;
				const float boxH = inMax.y - inMin.y;
				const float scale = (std::min)(boxW / static_cast<float>(row.loaded->width), boxH / static_cast<float>(row.loaded->height));
				const ImVec2 imageSize(static_cast<float>(row.loaded->width) * scale, static_cast<float>(row.loaded->height) * scale);
				const ImVec2 p0(inMin.x + (boxW - imageSize.x) * 0.5f, inMin.y + (boxH - imageSize.y) * 0.5f);
				drawList->AddImage(static_cast<ImTextureID>(row.loaded->gpuHandle), p0, ImVec2(p0.x + imageSize.x, p0.y + imageSize.y));
			}
			else
			{
				drawList->AddText(ImVec2(inMin.x + 4.0f, inMin.y + size * 0.4f), IM_COL32(140, 140, 140, 255), row.loaded ? "プレビュー不可" : "未読込");
			}
			if (row.loaded)
			{
				drawList->AddCircleFilled(ImVec2(inMax.x - 7.0f, inMin.y + 7.0f), 4.0f, row.loaded->referenceCount > 0 ? kColorReferenced : kColorUnreferenced);
			}
			if ((row.bootIndex >= 0) != row.saved)
			{
				drawList->AddRect(pos, ImVec2(pos.x + size, pos.y + size), kColorChanged, 0.0f, 0, 2.0f);
			}

			// 下の行: 起動時に読むチェックと名前
			bool boot = row.bootIndex >= 0;
			if (ImGui::Checkbox("##boot", &boot)) { setBoot(file, boot); }
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("起動時に読む"); }
			ImGui::SameLine(0.0f, 4.0f);
			const float nameWidth = (std::max)(size - checkWidth - 4.0f, 8.0f);
			const ImVec2 namePos = ImGui::GetCursorScreenPos();
			drawList->PushClipRect(namePos, ImVec2(namePos.x + nameWidth, namePos.y + checkWidth), true);
			drawList->AddText(ImVec2(namePos.x, namePos.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), file.name.c_str());
			drawList->PopClipRect();
			ImGui::Dummy(ImVec2(nameWidth, (std::max)(lineHeight, checkWidth)));
			ImGui::EndGroup();
			ImGui::PopID();
		}
		if (shown == 0) { ImGui::TextDisabled("このフォルダに画像はありません。"); }
	}
	ImGui::EndChild();

	// ---- 詳細: 選んだファイル ----
	if (ImGui::BeginChild("##AssetDetail", ImVec2(0.0f, 0.0f), true))
	{
		const auto selectedIt = std::find_if(assetFiles_.begin(), assetFiles_.end(), [this](const AssetFile& f) { return f.path == selectedAsset_; });
		if (selectedIt == assetFiles_.end())
		{
			ImGui::TextDisabled("画像を選ぶと、ここに詳細が出ます。");
		}
		else
		{
			const size_t index = static_cast<size_t>(selectedIt - assetFiles_.begin());
			const AssetFile& file = *selectedIt;
			const Row& row = rows[index];
			const float previewSize = (std::max)(ImGui::GetContentRegionAvail().y, 48.0f);
			if (row.loaded && row.loaded->previewable && row.loaded->gpuHandle != 0 && row.loaded->width > 0 && row.loaded->height > 0)
			{
				const float scale = (std::min)(previewSize / static_cast<float>(row.loaded->width), previewSize / static_cast<float>(row.loaded->height));
				ImGui::Image(static_cast<ImTextureID>(row.loaded->gpuHandle),
					ImVec2(static_cast<float>(row.loaded->width) * scale, static_cast<float>(row.loaded->height) * scale));
			}
			else
			{
				ImGui::Dummy(ImVec2(previewSize, previewSize));
				ImGui::GetWindowDrawList()->AddText(ImGui::GetItemRectMin(), IM_COL32(140, 140, 140, 255), row.loaded ? "プレビュー不可" : "未読込");
			}
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::TextUnformatted(file.name.c_str());
			ImGui::TextDisabled("textures/%s", file.relative.c_str());
			if (row.loaded)
			{
				ImGui::Text("%u x %u　%s", row.loaded->width, row.loaded->height, row.loaded->referenceCount > 0 ? "参照あり" : "未参照");
				if (row.loaded->referenceCount > 0) { ImGui::TextDisabled("最後の参照: %s", AgoText(row.loaded->lastReferenceFrame, frame).c_str()); }
			}
			else
			{
				ImGui::TextDisabled("未読み込み");
			}
			bool boot = row.bootIndex >= 0;
			if (ImGui::Checkbox("起動時に読む", &boot)) { setBoot(file, boot); }
			// 寿命は、表に載っている物だけ変えられる
			const int bootIndexNow = FindManifestIndex(BuildManifestKeys(textures_), file.key, ToLower(file.name), fileKeys);
			if (bootIndexNow >= 0)
			{
				TextureEntry& entry = textures_[static_cast<size_t>(bootIndexNow)];
				int lifetimeIndex = entry.lifetime == ResourceLifetime::Resident ? 0 : 1;
				ImGui::SetNextItemWidth(90.0f);
				if (ImGui::Combo("寿命", &lifetimeIndex, kLifetimeItems, IM_ARRAYSIZE(kLifetimeItems)))
				{
					entry.lifetime = lifetimeIndex == 0 ? ResourceLifetime::Resident : ResourceLifetime::Scene;
					dirty_ = true;
				}
			}
			if (!row.loaded)
			{
				if (ImGui::Button("今読み込む")) { TextureManager::GetInstance()->LoadTexture(file.path, ResourceLifetime::Scene); }
			}
			else
			{
				ImGui::BeginDisabled(row.loaded->referenceCount != 0);
				if (ImGui::Button("解放する…")) { requestRelease(*row.loaded); }
				ImGui::EndDisabled();
				if (row.loaded->referenceCount != 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) { ImGui::SetTooltip("使われているので、解放できません"); }
			}
			ImGui::EndGroup();
		}
	}
	ImGui::EndChild();
	ImGui::EndGroup();
}

void ResourceManifest::DrawModelsTab()
{
	using Loaded = TextureManager::LoadedTexture;
	static const char* const kLifetimeItems[] = { "常駐", "シーン" };
	const auto loaded = ModelManager::GetInstance()->GetLoadedModels();
	const auto textures = TextureManager::GetInstance()->GetLoadedTextures();
	const uint64_t frame = TimeManager::GetInstance().GetFrameCount();

	// モデルが使うテクスチャの、読み込み済みの情報を引く
	std::unordered_map<std::string, const Loaded*> textureByKey;
	for (const Loaded& t : textures) { textureByKey[t.path] = &t; }
	const auto findTexture = [&](const std::string& path) -> const Loaded*
	{
		const std::string key = NormalizeKey(path);
		if (const auto it = textureByKey.find(key); it != textureByKey.end()) { return it->second; }
		if (const auto it = textureByKey.find(key + "|linear"); it != textureByKey.end()) { return it->second; }
		return nullptr;
	};
	const auto previewable = [](const Loaded* t) { return t && t->previewable && t->gpuHandle != 0 && t->width > 0 && t->height > 0; };
	// 縦横比を保って、枠に収めて描く
	const auto drawFitted = [](ImDrawList* drawList, ImVec2 inMin, ImVec2 inMax, const Loaded& t)
	{
		const float boxW = inMax.x - inMin.x;
		const float boxH = inMax.y - inMin.y;
		const float scale = (std::min)(boxW / static_cast<float>(t.width), boxH / static_cast<float>(t.height));
		const ImVec2 imageSize(static_cast<float>(t.width) * scale, static_cast<float>(t.height) * scale);
		const ImVec2 p0(inMin.x + (boxW - imageSize.x) * 0.5f, inMin.y + (boxH - imageSize.y) * 0.5f);
		drawList->AddImage(static_cast<ImTextureID>(t.gpuHandle), p0, ImVec2(p0.x + imageSize.x, p0.y + imageSize.y));
	};

	// フォルダにある物と、読み込み済みの物（cube など、コードで読んだ物）を1つの一覧にまとめる
	struct Row
	{
		std::string name;
		std::string type;
		bool onDisk = false;
		const ModelManager::LoadedModel* info = nullptr;
		// 見た目に使うテクスチャ（無ければ nullptr）
		const Loaded* thumb = nullptr;
	};
	std::vector<Row> rows;
	for (const ModelEntry& candidate : ScanModelFiles()) { rows.push_back({ candidate.name, candidate.type, true, nullptr, nullptr }); }
	for (const auto& model : loaded)
	{
		const auto found = std::find_if(rows.begin(), rows.end(), [&](const Row& r) { return r.name == model.name; });
		if (found != rows.end()) { found->info = &model; }
		else { rows.push_back({ model.name, model.type, false, &model, nullptr }); }
	}
	std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.name < b.name; });
	for (Row& row : rows)
	{
		if (!row.info) { continue; }
		for (const std::string& path : row.info->texturePaths)
		{
			const Loaded* t = findTexture(path);
			if (previewable(t)) { row.thumb = t; break; }
		}
	}

	const auto findEntry = [](const std::vector<ModelEntry>& list, const std::string& name) -> const ModelEntry*
	{
		const auto it = std::find_if(list.begin(), list.end(), [&](const ModelEntry& m) { return m.name == name; });
		return it == list.end() ? nullptr : &*it;
	};
	const auto isChanged = [&](const std::string& name)
	{
		const ModelEntry* now = findEntry(models_, name);
		const ModelEntry* saved = findEntry(savedModels_, name);
		return (now == nullptr) != (saved == nullptr) || (now && saved && now->lifetime != saved->lifetime);
	};
	size_t changedCount = 0;
	for (const Row& row : rows) { if (isChanged(row.name)) { ++changedCount; } }

	// ---- 上の行: 件数と操作（テクスチャのタブと同じ並び） ----
	ImGui::Text("読み込み済み %zu 件 / 起動時に読む %zu 件", loaded.size(), models_.size());
	if (changedCount > 0)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColorChanged), "未保存の変更 %zu 件", changedCount);
	}
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat("大きさ##Thumbnail", &thumbnailSize_, 48.0f, 160.0f, "%.0f");
	ImGui::SameLine();
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColorReferenced), "●"); ImGui::SameLine(0.0f, 2.0f); ImGui::TextDisabled("参照あり");
	ImGui::SameLine();
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColorUnreferenced), "●"); ImGui::SameLine(0.0f, 2.0f); ImGui::TextDisabled("未参照");
	ImGui::SameLine();
	ImGui::TextDisabled("□ 起動時に読む");
	ImGui::SameLine();
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kColorChanged), "枠"); ImGui::SameLine(0.0f, 2.0f); ImGui::TextDisabled("未保存の変更");

	std::string removeName;
	std::string toggleOnName;
	std::string loadNowName;

	// ---- 本体: ファイルのグリッドと、詳細 ----
	ImGui::BeginGroup();
	const float detailHeight = ImGui::GetTextLineHeightWithSpacing() * 8.5f;
	const float gridHeight = (std::max)(ImGui::GetContentRegionAvail().y - detailHeight - ImGui::GetStyle().ItemSpacing.y, 60.0f);
	if (ImGui::BeginChild("##ModelGrid", ImVec2(0.0f, gridHeight), true))
	{
		ImGui::TextDisabled("%s", filter_[0] != '\0' ? "検索結果" : "models");
		const float size = thumbnailSize_;
		const ImGuiStyle& style = ImGui::GetStyle();
		const float lineHeight = ImGui::GetTextLineHeight();
		const float checkWidth = ImGui::GetFrameHeight();
		const int columns = (std::max)(1, static_cast<int>((ImGui::GetContentRegionAvail().x + style.ItemSpacing.x) / (size + style.ItemSpacing.x)));
		int shown = 0;
		for (const Row& row : rows)
		{
			if (!PassesFilter(row.name, filter_.data())) { continue; }
			const bool unused = row.info && row.info->referenceCount == 0;
			if (unusedOnly_ && !unused) { continue; }
			const ModelEntry* inManifest = findEntry(models_, row.name);
			const size_t usingObjects = row.info ? CountObjectsUsingModel(row.name) : 0;

			if (shown % columns != 0) { ImGui::SameLine(); }
			++shown;
			ImGui::PushID(row.name.c_str());
			ImGui::BeginGroup();
			const ImVec2 pos = ImGui::GetCursorScreenPos();
			if (ImGui::Selectable("##thumb", selectedModel_ == row.name, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(size, size)))
			{
				selectedModel_ = row.name;
				if (ImGui::IsMouseDoubleClicked(0) && !row.info && row.onDisk) { loadNowName = row.name; }
			}
			if (ImGui::BeginPopupContextItem("ModelMenu"))
			{
				selectedModel_ = row.name;
				bool boot = inManifest != nullptr;
				if (ImGui::MenuItem("起動時に読む", nullptr, &boot, row.onDisk))
				{
					if (boot) { toggleOnName = row.name; }
					else { removeName = row.name; }
				}
				if (ImGui::MenuItem("今読み込む（読み込み表には載せない）", nullptr, false, row.onDisk && !row.info)) { loadNowName = row.name; }
				if (ImGui::MenuItem("解放する…", nullptr, false, row.info && unused && usingObjects == 0))
				{
					releaseConfirmIsModel_ = true;
					releaseConfirmKey_ = row.name;
					releasePopupRequested_ = true;
				}
				if (row.info && (!unused || usingObjects != 0)) { ImGui::TextDisabled("※ 使われているので、解放できません"); }
				if (!row.onDisk) { ImGui::TextDisabled("※ コードで読む物なので、表には載せられません"); }
				ImGui::EndPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s (%s)\n%s\nテクスチャ %zu 枚", row.name.c_str(), row.type.c_str(),
					row.info ? "読み込み済み" : "未読み込み（ダブルクリックで読む）", row.info ? row.info->texturePaths.size() : size_t(0));
			}

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const ImVec2 inMin(pos.x + 2.0f, pos.y + 2.0f);
			const ImVec2 inMax(pos.x + size - 2.0f, pos.y + size - 2.0f);
			drawList->AddRectFilled(inMin, inMax, IM_COL32(36, 36, 36, 255));
			if (row.thumb) { drawFitted(drawList, inMin, inMax, *row.thumb); }
			else
			{
				const char* text = !row.info ? "未読込" : (row.info->texturePaths.empty() ? "画像なし" : "プレビュー不可");
				drawList->AddText(ImVec2(inMin.x + 4.0f, inMin.y + size * 0.4f), IM_COL32(140, 140, 140, 255), text);
			}
			if (row.info)
			{
				drawList->AddCircleFilled(ImVec2(inMax.x - 7.0f, inMin.y + 7.0f), 4.0f, row.info->referenceCount > 0 ? kColorReferenced : kColorUnreferenced);
			}
			if (isChanged(row.name))
			{
				drawList->AddRect(pos, ImVec2(pos.x + size, pos.y + size), kColorChanged, 0.0f, 0, 2.0f);
			}

			// 下の行: 起動時に読むチェックと名前
			bool boot = inManifest != nullptr;
			ImGui::BeginDisabled(!row.onDisk);
			if (ImGui::Checkbox("##boot", &boot))
			{
				if (boot) { toggleOnName = row.name; }
				else { removeName = row.name; }
			}
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) { ImGui::SetTooltip(row.onDisk ? "起動時に読む" : "コードで読む物なので、表には載せられません"); }
			ImGui::SameLine(0.0f, 4.0f);
			const float nameWidth = (std::max)(size - checkWidth - 4.0f, 8.0f);
			const ImVec2 namePos = ImGui::GetCursorScreenPos();
			drawList->PushClipRect(namePos, ImVec2(namePos.x + nameWidth, namePos.y + checkWidth), true);
			drawList->AddText(ImVec2(namePos.x, namePos.y + style.FramePadding.y), ImGui::GetColorU32(ImGuiCol_Text), row.name.c_str());
			drawList->PopClipRect();
			ImGui::Dummy(ImVec2(nameWidth, (std::max)(lineHeight, checkWidth)));
			ImGui::EndGroup();
			ImGui::PopID();
		}
		if (shown == 0) { ImGui::TextDisabled("モデルはありません。"); }
	}
	ImGui::EndChild();

	// ---- 詳細: 選んだモデル ----
	if (ImGui::BeginChild("##ModelDetail", ImVec2(0.0f, 0.0f), true))
	{
		const auto selectedIt = std::find_if(rows.begin(), rows.end(), [this](const Row& r) { return r.name == selectedModel_; });
		if (selectedIt == rows.end())
		{
			ImGui::TextDisabled("モデルを選ぶと、ここに詳細が出ます。");
		}
		else
		{
			const Row& row = *selectedIt;
			const bool unused = row.info && row.info->referenceCount == 0;
			const size_t usingObjects = row.info ? CountObjectsUsingModel(row.name) : 0;
			const float previewSize = (std::max)(ImGui::GetContentRegionAvail().y, 48.0f);
			if (row.thumb)
			{
				const float scale = (std::min)(previewSize / static_cast<float>(row.thumb->width), previewSize / static_cast<float>(row.thumb->height));
				ImGui::Image(static_cast<ImTextureID>(row.thumb->gpuHandle),
					ImVec2(static_cast<float>(row.thumb->width) * scale, static_cast<float>(row.thumb->height) * scale));
			}
			else
			{
				ImGui::Dummy(ImVec2(previewSize, previewSize));
				ImGui::GetWindowDrawList()->AddText(ImGui::GetItemRectMin(), IM_COL32(140, 140, 140, 255), row.info ? "画像なし" : "未読込");
			}
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::TextUnformatted(row.name.c_str());
			ImGui::TextDisabled("models/%s　%s", row.name.c_str(), row.type.c_str());
			if (row.info)
			{
				ImGui::Text("%s　使用中 %zu 個", unused ? "未参照" : "参照あり", usingObjects);
				if (!unused) { ImGui::TextDisabled("最後の参照: %s", AgoText(row.info->lastReferenceFrame, frame).c_str()); }
			}
			else { ImGui::TextDisabled("未読み込み"); }
			const ModelEntry* inManifest = findEntry(models_, row.name);
			bool boot = inManifest != nullptr;
			ImGui::BeginDisabled(!row.onDisk);
			if (ImGui::Checkbox("起動時に読む", &boot))
			{
				if (boot) { toggleOnName = row.name; }
				else { removeName = row.name; }
			}
			ImGui::EndDisabled();
			// 寿命は、表に載っている物だけ変えられる
			if (inManifest)
			{
				int lifetimeIndex = inManifest->lifetime == ResourceLifetime::Resident ? 0 : 1;
				ImGui::SetNextItemWidth(90.0f);
				if (ImGui::Combo("寿命", &lifetimeIndex, kLifetimeItems, IM_ARRAYSIZE(kLifetimeItems)))
				{
					for (ModelEntry& m : models_)
					{
						if (m.name == row.name) { m.lifetime = lifetimeIndex == 0 ? ResourceLifetime::Resident : ResourceLifetime::Scene; }
					}
					dirty_ = true;
				}
			}
			if (!row.info)
			{
				ImGui::BeginDisabled(!row.onDisk);
				if (ImGui::Button("今読み込む")) { loadNowName = row.name; }
				ImGui::EndDisabled();
			}
			else
			{
				const bool inUse = !unused || usingObjects != 0;
				ImGui::BeginDisabled(inUse);
				if (ImGui::Button("解放する…"))
				{
					releaseConfirmIsModel_ = true;
					releaseConfirmKey_ = row.name;
					releasePopupRequested_ = true;
				}
				ImGui::EndDisabled();
				if (inUse && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) { ImGui::SetTooltip("使われているので、解放できません"); }
			}
			ImGui::EndGroup();

			// 使っているテクスチャ
			ImGui::SameLine();
			ImGui::BeginGroup();
			const size_t textureCount = row.info ? row.info->texturePaths.size() : 0;
			ImGui::TextDisabled("使っているテクスチャ %zu 枚", textureCount);
			if (row.info)
			{
				const float thumbSize = 56.0f;
				int index = 0;
				for (const std::string& path : row.info->texturePaths)
				{
					const Loaded* t = findTexture(path);
					if (index > 0 && index % 6 != 0) { ImGui::SameLine(); }
					ImGui::PushID(index++);
					ImGui::Dummy(ImVec2(thumbSize, thumbSize));
					const ImVec2 mn = ImGui::GetItemRectMin();
					const ImVec2 mx = ImGui::GetItemRectMax();
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					drawList->AddRectFilled(mn, mx, IM_COL32(36, 36, 36, 255));
					if (previewable(t)) { drawFitted(drawList, mn, mx, *t); }
					else { drawList->AddText(ImVec2(mn.x + 4.0f, mn.y + thumbSize * 0.35f), IM_COL32(140, 140, 140, 255), "不可"); }
					if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", DisplayTexturePath(path).c_str()); }
					ImGui::PopID();
				}
			}
			ImGui::EndGroup();
		}
	}
	ImGui::EndChild();
	ImGui::EndGroup();

	if (!removeName.empty()) { RemoveModel(removeName); }
	if (!toggleOnName.empty())
	{
		const auto it = std::find_if(rows.begin(), rows.end(), [&](const Row& r) { return r.name == toggleOnName; });
		if (it != rows.end())
		{
			ModelEntry entry;
			entry.name = it->name;
			entry.type = it->type;
			entry.lifetime = ResourceLifetime::Resident;
			statusMessage_ = AddModel(entry) ? "起動時に読むようにしました: " + entry.name : "読み込めませんでした: " + entry.name;
		}
	}
	if (!loadNowName.empty())
	{
		const auto it = std::find_if(rows.begin(), rows.end(), [&](const Row& r) { return r.name == loadNowName; });
		if (it != rows.end()) { ModelManager::GetInstance()->LoadModel(it->name, it->type, ResourceLifetime::Scene); }
	}
}
#endif
} // namespace KCE
