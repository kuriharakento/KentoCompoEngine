#include "stage/StageManager.h"

#include <fstream>
#include <unordered_map>
#include "base/Camera.h"
#include "base/Logger.h"
#include "base/PathManager.h"
#include "core/SchemaVersion.h"
#include "editor/SelectionContext.h"
#include "editor/command/CommandHistory.h"
#include "graphics/text/Text3DRenderer.h"
#include "manager/scene/CameraManager.h"
#include "manager/editor/DebugUIManager.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
namespace
{
constexpr const char* kStageDirectory = "json/stage";
constexpr float kCreateDistance = 5.0f;
uint64_t gNextLifetimeId = 1;
std::unordered_map<const StageManager*, uint64_t> gLiveManagers;

class CreateStageTextCommand final : public ICommand
{
public:
	CreateStageTextCommand(StageManager* manager, std::string name, std::unique_ptr<TextMesh3D> mesh)
		: manager_(manager), lifetimeId_(manager ? manager->GetLifetimeId() : 0), name_(std::move(name)), mesh_(std::move(mesh)) {}
	void Execute() override { if (StageManager::IsAlive(manager_, lifetimeId_) && mesh_) { manager_->AddText3D(name_, std::move(mesh_)); } }
	void Undo() override { if (StageManager::IsAlive(manager_, lifetimeId_)) { mesh_ = manager_->RemoveText3D(name_); } }
	std::string GetName() const override { return "Create 3D Text"; }
private:
	// シーン所有。履歴はシーン切り替え時に破棄される前提
	StageManager* manager_ = nullptr;
	uint64_t lifetimeId_ = 0;
	std::string name_;
	std::unique_ptr<TextMesh3D> mesh_;
};

class DeleteStageTextCommand final : public ICommand
{
public:
	DeleteStageTextCommand(StageManager* manager, std::string name)
		: manager_(manager), lifetimeId_(manager ? manager->GetLifetimeId() : 0), name_(std::move(name)) {}
	void Execute() override { if (StageManager::IsAlive(manager_, lifetimeId_)) { mesh_ = manager_->RemoveText3D(name_); } }
	void Undo() override { if (StageManager::IsAlive(manager_, lifetimeId_) && mesh_) { manager_->AddText3D(name_, std::move(mesh_)); } }
	std::string GetName() const override { return "Delete 3D Text"; }
private:
	// シーン所有。履歴はシーン切り替え時に破棄される前提
	StageManager* manager_ = nullptr;
	uint64_t lifetimeId_ = 0;
	std::string name_;
	std::unique_ptr<TextMesh3D> mesh_;
};

nlohmann::ordered_json SerializeVector3(const Vector3& value) { return { value.x, value.y, value.z }; }
nlohmann::ordered_json SerializeVector4(const Vector4& value) { return { value.x, value.y, value.z, value.w }; }
bool ReadFloatArray(const nlohmann::json& json, size_t count, float* output)
{
	if (!json.is_array() || json.size() != count) { return false; }
	for (size_t i = 0; i < count; ++i)
	{
		if (!json[i].is_number()) { return false; }
		output[i] = json[i].get<float>();
	}
	return true;
}
} // namespace

StageManager::~StageManager()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance()) { DebugUIManager::GetInstance()->Unregister(this); }
#endif
	Clear();
	gLiveManagers.erase(this);
}

bool StageManager::IsAlive(const StageManager* manager, uint64_t lifetimeId)
{
	const auto found = gLiveManagers.find(manager);
	return found != gLiveManagers.end() && found->second == lifetimeId;
}

void StageManager::Initialize(const std::string& stageName, Text3DRenderer* text3DRenderer, CameraManager* cameraManager)
{
	stageName_ = stageName;
	lifetimeId_ = gNextLifetimeId++;
	gLiveManagers[this] = lifetimeId_;
	text3DRenderer_ = text3DRenderer;
	cameraManager_ = cameraManager;
#ifdef USE_IMGUI
	RegisterDebugUI();
#endif
	LoadFromFile();
}

bool StageManager::AddText3D(const std::string& name, std::unique_ptr<TextMesh3D> mesh)
{
	if (name.empty() || !mesh || !text3DRenderer_ || text3DRenderer_->Find(name)) { return false; }
	text3DRenderer_->Register(name, mesh.get());
	texts3D_.push_back({ name, std::move(mesh) });
	return true;
}

std::unique_ptr<TextMesh3D> StageManager::RemoveText3D(const std::string& name)
{
	for (auto it = texts3D_.begin(); it != texts3D_.end(); ++it)
	{
		if (it->name != name) { continue; }
		text3DRenderer_->Unregister(it->mesh.get());
		std::unique_ptr<TextMesh3D> mesh = std::move(it->mesh);
		texts3D_.erase(it);
		const SelectionItem& selected = SelectionContext::GetInstance()->GetPrimary();
		if (selected.kind == SelectionKind::Text3D && selected.name == name) { SelectionContext::GetInstance()->ClearSelection(); }
		return mesh;
	}
	return nullptr;
}

bool StageManager::OwnsText3D(const std::string& name) const
{
	for (const TextEntry& entry : texts3D_) { if (entry.name == name) { return true; } }
	return false;
}

void StageManager::Clear()
{
	if (text3DRenderer_) { for (const TextEntry& entry : texts3D_) { text3DRenderer_->Unregister(entry.mesh.get()); } }
	texts3D_.clear();
}

std::filesystem::path StageManager::GetFilePath() const
{
	return PathManager::GetApplicationResourceRoot() / kStageDirectory / (stageName_ + ".json");
}

nlohmann::ordered_json StageManager::Serialize() const
{
	nlohmann::ordered_json json;
	json["version"] = kStageSchemaVersion;
	json["texts3d"] = nlohmann::ordered_json::array();
	for (const TextEntry& entry : texts3D_)
	{
		const TextMesh3D::Params& params = entry.mesh->GetParams();
		nlohmann::ordered_json text;
		text["name"] = entry.name;
		text["text"] = entry.mesh->GetText();
		text["position"] = SerializeVector3(params.position);
		text["rotation"] = { params.rotation.x, params.rotation.y, params.rotation.z, params.rotation.w };
		text["scale"] = params.scale;
		text["size"] = params.size;
		text["color"] = SerializeVector4(params.color);
		text["style"] = TextAppearStyleToString(params.style);
		json["texts3d"].push_back(std::move(text));
	}
	return json;
}

bool StageManager::Deserialize(const nlohmann::json& json, std::string& outError)
{
	if (!json.is_object()) { outError = "ルートがオブジェクトではありません"; return false; }
	if (!json.contains("version") || !json["version"].is_number_integer()) { outError = "version がありません"; return false; }
	const int version = json["version"].get<int>();
	if (!IsLoadableSchemaVersion(version, kStageSchemaVersion))
	{
		outError = "対応していないスキーマバージョンです (file=" + std::to_string(version) + ", engine=" + std::to_string(kStageSchemaVersion) + ")";
		return false;
	}
	Clear();
	if (!json.contains("texts3d")) { return true; }
	if (!json["texts3d"].is_array()) { outError = "texts3d が配列ではありません"; return false; }
	for (const auto& text : json["texts3d"])
	{
		if (!text.is_object() || !text.contains("name") || !text["name"].is_string() || !text.contains("text") || !text["text"].is_string())
		{
			Logger::Log("StageManager: 不正な 3D 文字を読み飛ばしました\n", Logger::LogLevel::Warning);
			continue;
		}
		const std::string name = text["name"].get<std::string>();
		if (name.empty() || !text3DRenderer_ || text3DRenderer_->Find(name))
		{
			Logger::Log("StageManager: 重複または空の名前を読み飛ばしました: " + name + "\n", Logger::LogLevel::Warning);
			continue;
		}
		auto mesh = std::make_unique<TextMesh3D>();
		mesh->SetText(text["text"].get<std::string>());
		TextMesh3D::Params& params = mesh->GetParams();
		if (text.contains("position")) { ReadFloatArray(text["position"], 3, &params.position.x); }
		if (text.contains("rotation")) { ReadFloatArray(text["rotation"], 4, &params.rotation.x); }
		if (text.contains("scale") && text["scale"].is_number()) { params.scale = text["scale"].get<float>(); }
		if (text.contains("size") && text["size"].is_number()) { params.size = text["size"].get<float>(); }
		if (text.contains("color")) { ReadFloatArray(text["color"], 4, &params.color.x); }
		if (text.contains("style") && text["style"].is_string()) { params.style = TextAppearStyleFromString(text["style"].get<std::string>()); }
		params.reveal = TextMesh3D::kRevealAll;
		params.exit = 0.0f;
		AddText3D(name, std::move(mesh));
	}
	return true;
}

bool StageManager::SaveToFile()
{
	const std::filesystem::path fullPath = GetFilePath();
	std::error_code error;
	std::filesystem::create_directories(fullPath.parent_path(), error);
	const std::filesystem::path tempPath = fullPath.string() + ".tmp";
	const std::string contents = Serialize().dump(4);
	{
		std::ofstream output(tempPath);
		if (!output) { Logger::Log("StageManager: 保存先を開けませんでした: " + tempPath.string() + "\n", Logger::LogLevel::Error); return false; }
		output << contents;
		if (!output) { Logger::Log("StageManager: 書き込みに失敗しました: " + tempPath.string() + "\n", Logger::LogLevel::Error); return false; }
	}
	std::filesystem::rename(tempPath, fullPath, error);
	if (error)
	{
		std::filesystem::remove(fullPath, error);
		std::filesystem::rename(tempPath, fullPath, error);
		if (error) { Logger::Log("StageManager: 保存の確定に失敗しました: " + error.message() + "\n", Logger::LogLevel::Error); return false; }
	}
	savedState_ = contents;
	return true;
}

bool StageManager::LoadFromFile()
{
	const std::filesystem::path fullPath = GetFilePath();
	if (!std::filesystem::exists(fullPath))
	{
		Clear();
		savedState_ = Serialize().dump(4);
		Logger::Log("StageManager: 新しい空のステージを開きました: " + stageName_ + "\n", Logger::LogLevel::Info);
		return true;
	}
	std::ifstream input(fullPath);
	if (!input) { Logger::Log("StageManager: ファイルを開けません: " + fullPath.string() + "\n", Logger::LogLevel::Error); Clear(); return false; }
	try
	{
		nlohmann::json json;
		input >> json;
		std::string error;
		if (!Deserialize(json, error))
		{
			Logger::Log("StageManager: 読み込みに失敗しました: " + error + "\n", Logger::LogLevel::Error);
			Clear();
			return false;
		}
	}
	catch (const nlohmann::json::exception& e)
	{
		Logger::Log(std::string("StageManager: JSON の解析に失敗しました: ") + e.what() + "\n", Logger::LogLevel::Error);
		Clear();
		return false;
	}
	savedState_ = Serialize().dump(4);
	return true;
}

bool StageManager::IsDirty() const { return Serialize().dump(4) != savedState_; }

#ifdef USE_IMGUI
void StageManager::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterHierarchySection(this, "ステージ", [this]() { DrawHierarchyImGui(); });
	DebugUIManager::GetInstance()->RegisterInspector(this, SelectionKind::Text3D, [this](const SelectionItem& item) { DrawInspectorImGui(item); });
}

void StageManager::DrawHierarchyImGui()
{
	ImGui::Text("%s%s", stageName_.c_str(), IsDirty() ? " *" : "");
	ImGui::SameLine();
	if (ImGui::Button("保存")) { SaveToFile(); }
	if (ImGui::Button("3D テキストを作る"))
	{
		newName_.fill('\0'); newText_.fill('\0'); createError_.clear(); createPopupRequested_ = true;
	}
	if (createPopupRequested_)
	{
		ImGui::OpenPopup("3D テキストを作る###CreateStageText3D");
		createPopupRequested_ = false;
	}
	if (ImGui::BeginPopupModal("3D テキストを作る###CreateStageText3D", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("名前", newName_.data(), newName_.size());
		ImGui::InputTextMultiline("文字", newText_.data(), newText_.size());
		if (!createError_.empty()) { ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.25f, 1.0f), "%s", createError_.c_str()); }
		if (ImGui::Button("作る"))
		{
			const std::string name = newName_.data();
			if (name.empty()) { createError_ = "名前を入力してください。"; }
			else if (!text3DRenderer_ || text3DRenderer_->Find(name)) { createError_ = "同じ名前の 3D テキストがあります。"; }
			else
			{
				auto mesh = std::make_unique<TextMesh3D>();
				mesh->SetText(newText_.data());
				if (Camera* camera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr)
				{
					const Matrix4x4& world = camera->GetWorldMatrix();
					const Vector3 forward = { world.m[2][0], world.m[2][1], world.m[2][2] };
					mesh->GetParams().position = camera->GetTranslate() + forward * kCreateDistance;
				}
				CommandHistory::GetInstance()->Execute(std::make_unique<CreateStageTextCommand>(this, name, std::move(mesh)));
				SelectionItem item; item.kind = SelectionKind::Text3D; item.name = name;
				SelectionContext::GetInstance()->Select(item);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル")) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
}

void StageManager::DrawInspectorImGui(const SelectionItem& item)
{
	if (!OwnsText3D(item.name)) { return; }
	ImGui::Separator();
	if (ImGui::Button("ステージから消す"))
	{
		CommandHistory::GetInstance()->Execute(std::make_unique<DeleteStageTextCommand>(this, item.name));
	}
}
#endif
} // namespace KCE
