#include "stage/StageManager.h"

#include <fstream>
#include <algorithm>
#include <numbers>
#include <unordered_map>
#include "base/Camera.h"
#include "base/Logger.h"
#include "base/PathManager.h"
#include "core/SchemaVersion.h"
#include "editor/SelectionContext.h"
#include "editor/command/CommandHistory.h"
#include "graphics/text/Text3DRenderer.h"
#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"
#include "graphics/3d/IRenderable3d.h"
#include "graphics/view/StageMonitor.h"
#include "graphics/view/ISubViewProvider.h"
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
constexpr const char* kMonitorScreenModel = "plane";
constexpr Vector3 kMonitorScreenScale = { 1.2f, 0.675f, 1.0f };
constexpr Vector3 kMonitorScreenRotation = { 0.0f, std::numbers::pi_v<float>, 0.0f };
constexpr float kTransformDragSpeed = 0.05f;
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

class CreateStageMonitorCommand final : public ICommand
{
public:
	CreateStageMonitorCommand(StageManager* manager, std::unique_ptr<StageManager::MonitorEntry> entry)
		: manager_(manager), lifetimeId_(manager ? manager->GetLifetimeId() : 0), name_(entry ? entry->name : std::string()), entry_(std::move(entry)) {}
	void Execute() override
	{
		// 失敗したら entry_ は手元に残る。消さずに持っておき、次の Redo でやり直せるようにする
		if (StageManager::IsAlive(manager_, lifetimeId_) && entry_ && !manager_->AddStageMonitor(entry_))
		{
			Logger::Log("StageManager: モニターを戻せませんでした: " + name_ + "\n", Logger::LogLevel::Warning);
		}
	}
	void Undo() override { if (StageManager::IsAlive(manager_, lifetimeId_)) { entry_ = manager_->RemoveStageMonitor(name_); } }
	std::string GetName() const override { return "Create Stage Monitor"; }
private:
	// シーン所有。世代 ID が一致する間だけ使う
	StageManager* manager_ = nullptr;
	uint64_t lifetimeId_ = 0;
	std::string name_;
	std::unique_ptr<StageManager::MonitorEntry> entry_;
};

class DeleteStageMonitorCommand final : public ICommand
{
public:
	DeleteStageMonitorCommand(StageManager* manager, std::string name)
		: manager_(manager), lifetimeId_(manager ? manager->GetLifetimeId() : 0), name_(std::move(name)) {}
	void Execute() override { if (StageManager::IsAlive(manager_, lifetimeId_)) { entry_ = manager_->RemoveStageMonitor(name_); } }
	void Undo() override
	{
		// 失敗したら entry_ は手元に残る（例: モニターのカメラを見ていて、消したときにカメラが残った）
		if (StageManager::IsAlive(manager_, lifetimeId_) && entry_ && !manager_->AddStageMonitor(entry_))
		{
			Logger::Log("StageManager: 消したモニターを戻せませんでした: " + name_ + "\n", Logger::LogLevel::Warning);
		}
	}
	std::string GetName() const override { return "Delete Stage Monitor"; }
private:
	// シーン所有。世代 ID が一致する間だけ使う
	StageManager* manager_ = nullptr;
	uint64_t lifetimeId_ = 0;
	std::string name_;
	std::unique_ptr<StageManager::MonitorEntry> entry_;
};

class EditStageMonitorCommand final : public ICommand
{
public:
	EditStageMonitorCommand(StageManager* manager, std::string name, const StageManager::MonitorState& before,
		const StageManager::MonitorState& after, bool recreateView)
		: manager_(manager), lifetimeId_(manager ? manager->GetLifetimeId() : 0), name_(std::move(name)),
		before_(before), after_(after), recreateView_(recreateView) {}
	void Execute() override { Apply(after_); }
	void Undo() override { Apply(before_); }
	std::string GetName() const override { return "Edit Stage Monitor"; }
private:
	void Apply(const StageManager::MonitorState& state)
	{
		if (StageManager::IsAlive(manager_, lifetimeId_)) { manager_->ApplyMonitorState(name_, state, recreateView_); }
	}
	// シーン所有。世代 ID が一致する間だけ使う
	StageManager* manager_ = nullptr;
	uint64_t lifetimeId_ = 0;
	std::string name_;
	StageManager::MonitorState before_{};
	StageManager::MonitorState after_{};
	bool recreateView_ = false;
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

/** @brief 保存する項目だけを比べる。reveal / exit はシーケンスが動かす値なので保存も比較もしない */
bool SameSavedParams(const TextMesh3D::Params& a, const TextMesh3D::Params& b)
{
	return a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z &&
		a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y && a.rotation.z == b.rotation.z && a.rotation.w == b.rotation.w &&
		a.scale == b.scale && a.size == b.size &&
		a.color.x == b.color.x && a.color.y == b.color.y && a.color.z == b.color.z && a.color.w == b.color.w &&
		a.style == b.style;
}

bool SameVector3(const Vector3& a, const Vector3& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool SameMonitorState(const StageManager::MonitorState& a, const StageManager::MonitorState& b)
{
	return SameVector3(a.screenPosition, b.screenPosition) && SameVector3(a.screenRotation, b.screenRotation)
		&& SameVector3(a.screenScale, b.screenScale) && SameVector3(a.cameraPosition, b.cameraPosition)
		&& SameVector3(a.cameraRotation, b.cameraRotation) && a.width == b.width && a.height == b.height
		&& a.framesPerSecond == b.framesPerSecond;
}

StageManager::MonitorState GetCurrentMonitorState(const StageManager::MonitorEntry& entry)
{
	StageManager::MonitorState state = entry.state;
	if (entry.screen)
	{
		state.screenPosition = entry.screen->GetPosition();
		state.screenRotation = entry.screen->GetRotation();
		state.screenScale = entry.screen->GetScale();
	}
	if (entry.monitor && entry.monitor->GetCamera())
	{
		state.cameraPosition = entry.monitor->GetCamera()->GetTranslate();
		state.cameraRotation = entry.monitor->GetCamera()->GetRotate();
	}
	return state;
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

void StageManager::Initialize(const std::string& stageName, Text3DRenderer* text3DRenderer, CameraManager* cameraManager,
	ISubViewProvider* subViewProvider, Object3dCommon* object3dCommon, LightManager* lightManager)
{
	stageName_ = stageName;
	lifetimeId_ = gNextLifetimeId++;
	gLiveManagers[this] = lifetimeId_;
	text3DRenderer_ = text3DRenderer;
	cameraManager_ = cameraManager;
	subViewProvider_ = subViewProvider;
	object3dCommon_ = object3dCommon;
	lightManager_ = lightManager;
#ifdef USE_IMGUI
	RegisterDebugUI();
#endif
	LoadFromFile();
}

void StageManager::Update()
{
	for (const auto& entry : monitors_)
	{
		if (entry->monitor) { entry->monitor->Update(); }
	}
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
	for (auto& entry : monitors_)
	{
		// 画面のテクスチャ参照とサブビューを先に外してから GameObject を破棄する
		entry->monitor.reset();
		if (GameObjectManager::HasInstance()) { GameObjectManager::GetInstance()->Unregister(entry->screen.get()); }
		entry->screen.reset();
	}
	monitors_.clear();
}

StageManager::MonitorEntry* StageManager::FindMonitor(const std::string& name)
{
	for (const auto& entry : monitors_) { if (entry->name == name) { return entry.get(); } }
	return nullptr;
}

const StageManager::MonitorEntry* StageManager::FindMonitor(const std::string& name) const
{
	for (const auto& entry : monitors_) { if (entry->name == name) { return entry.get(); } }
	return nullptr;
}

std::unique_ptr<StageManager::MonitorEntry> StageManager::CreateMonitor(
	const std::string& name, const std::string& cameraName, const MonitorState& state)
{
	if (name.empty() || cameraName.empty() || !subViewProvider_ || !cameraManager_ || !object3dCommon_ || !lightManager_)
	{
		return nullptr;
	}
	auto entry = std::make_unique<MonitorEntry>();
	entry->name = name;
	entry->cameraName = cameraName;
	entry->state = state;
	entry->screen = std::make_unique<GameObject>("StageMonitor");
	entry->screen->Initialize(object3dCommon_, lightManager_);
	entry->screen->SetName(name);
	entry->screen->SetModel(kMonitorScreenModel);
	entry->screen->SetPosition(state.screenPosition);
	entry->screen->SetRotation(state.screenRotation);
	entry->screen->SetScale(state.screenScale);
	if (auto* renderable = entry->screen->GetRenderable3d())
	{
		renderable->SetRenderingType(RenderingType::Forward);
		renderable->SetEnableLighting(false);
	}
	return entry;
}

bool StageManager::AddStageMonitor(std::unique_ptr<MonitorEntry>& entry)
{
	if (!entry || FindMonitor(entry->name) || !cameraManager_ || cameraManager_->GetCamera(entry->cameraName)
		|| !GameObjectManager::HasInstance() || GameObjectManager::GetInstance()->Find(entry->name))
	{
		return false;
	}
	entry->state.width = (std::clamp)(entry->state.width, kMinMonitorResolution, kMaxMonitorResolution);
	entry->state.height = (std::clamp)(entry->state.height, kMinMonitorResolution, kMaxMonitorResolution);
	entry->state.framesPerSecond = (std::clamp)(entry->state.framesPerSecond, kMinMonitorFramesPerSecond, kMaxMonitorFramesPerSecond);
	entry->monitor = std::make_unique<StageMonitor>();
	if (!entry->monitor->Initialize(subViewProvider_, cameraManager_, entry->cameraName, entry->state.width, entry->state.height))
	{
		entry->monitor.reset();
		return false;
	}
	entry->monitor->GetCamera()->SetTranslate(entry->state.cameraPosition);
	entry->monitor->GetCamera()->SetRotate(entry->state.cameraRotation);
	entry->monitor->SetFramesPerSecond(entry->state.framesPerSecond);
	entry->monitor->SetScreen(entry->screen.get());
	GameObjectManager::GetInstance()->Register(entry->screen.get());
	monitors_.push_back(std::move(entry));
	return true;
}

std::unique_ptr<StageManager::MonitorEntry> StageManager::RemoveStageMonitor(const std::string& name)
{
	for (auto it = monitors_.begin(); it != monitors_.end(); ++it)
	{
		if ((*it)->name != name) { continue; }
		std::unique_ptr<MonitorEntry> entry = std::move(*it);
		monitors_.erase(it);
		// カメラはモニターと一緒に消えるので、Undo で同じ所に戻せるよう今の位置を覚えてから畳む
		entry->state = GetCurrentMonitorState(*entry);
		entry->monitor.reset();
		if (GameObjectManager::HasInstance()) { GameObjectManager::GetInstance()->Unregister(entry->screen.get()); }
		const SelectionItem& selected = SelectionContext::GetInstance()->GetPrimary();
		if (selected.kind == SelectionKind::StageMonitor && selected.name == name) { SelectionContext::GetInstance()->ClearSelection(); }
		return entry;
	}
	return nullptr;
}

bool StageManager::ApplyMonitorState(const std::string& name, const MonitorState& state, bool recreateView)
{
	MonitorEntry* entry = FindMonitor(name);
	if (!entry || !entry->screen || !entry->monitor || !entry->monitor->GetCamera()) { return false; }
	MonitorState applied = state;
	applied.width = (std::clamp)(applied.width, kMinMonitorResolution, kMaxMonitorResolution);
	applied.height = (std::clamp)(applied.height, kMinMonitorResolution, kMaxMonitorResolution);
	applied.framesPerSecond = (std::clamp)(applied.framesPerSecond, kMinMonitorFramesPerSecond, kMaxMonitorFramesPerSecond);
	entry->screen->SetPosition(applied.screenPosition);
	entry->screen->SetRotation(applied.screenRotation);
	entry->screen->SetScale(applied.screenScale);
	entry->monitor->GetCamera()->SetTranslate(applied.cameraPosition);
	entry->monitor->GetCamera()->SetRotate(applied.cameraRotation);
	if (recreateView && !entry->monitor->RecreateView(applied.width, applied.height)) { return false; }
	entry->monitor->SetFramesPerSecond(applied.framesPerSecond);
	entry->state = applied;
	return true;
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
	json["monitors"] = nlohmann::ordered_json::array();
	for (const auto& entry : monitors_)
	{
		const MonitorState state = GetCurrentMonitorState(*entry);
		nlohmann::ordered_json monitor;
		monitor["name"] = entry->name;
		monitor["screen"] = {
			{ "position", SerializeVector3(state.screenPosition) },
			{ "rotation", SerializeVector3(state.screenRotation) },
			{ "scale", SerializeVector3(state.screenScale) }
		};
		monitor["camera"] = {
			{ "name", entry->cameraName },
			{ "position", SerializeVector3(state.cameraPosition) },
			{ "rotation", SerializeVector3(state.cameraRotation) }
		};
		monitor["resolution"] = { state.width, state.height };
		monitor["framesPerSecond"] = state.framesPerSecond;
		json["monitors"].push_back(std::move(monitor));
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
	if (json.contains("texts3d") && !json["texts3d"].is_array()) { outError = "texts3d が配列ではありません"; return false; }
	if (json.contains("texts3d"))
	{
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
	}

	if (json.contains("monitors") && !json["monitors"].is_array()) { outError = "monitors が配列ではありません"; return false; }
	if (json.contains("monitors"))
	{
		for (const auto& monitor : json["monitors"])
		{
			if (!monitor.is_object() || !monitor.contains("name") || !monitor["name"].is_string()
				|| !monitor.contains("screen") || !monitor["screen"].is_object()
				|| !monitor.contains("camera") || !monitor["camera"].is_object())
			{
				Logger::Log("StageManager: 不正なモニターを読み飛ばしました\n", Logger::LogLevel::Warning);
				continue;
			}
			const std::string name = monitor["name"].get<std::string>();
			const auto& screen = monitor["screen"];
			const auto& camera = monitor["camera"];
			if (!camera.contains("name") || !camera["name"].is_string())
			{
				Logger::Log("StageManager: カメラ名のないモニターを読み飛ばしました: " + name + "\n", Logger::LogLevel::Warning);
				continue;
			}
			MonitorState state;
			state.screenScale = kMonitorScreenScale;
			if (screen.contains("position")) { ReadFloatArray(screen["position"], 3, &state.screenPosition.x); }
			if (screen.contains("rotation")) { ReadFloatArray(screen["rotation"], 3, &state.screenRotation.x); }
			if (screen.contains("scale")) { ReadFloatArray(screen["scale"], 3, &state.screenScale.x); }
			if (camera.contains("position")) { ReadFloatArray(camera["position"], 3, &state.cameraPosition.x); }
			if (camera.contains("rotation")) { ReadFloatArray(camera["rotation"], 3, &state.cameraRotation.x); }
			if (monitor.contains("resolution") && monitor["resolution"].is_array() && monitor["resolution"].size() == 2
				&& monitor["resolution"][0].is_number_unsigned() && monitor["resolution"][1].is_number_unsigned())
			{
				state.width = monitor["resolution"][0].get<uint32_t>();
				state.height = monitor["resolution"][1].get<uint32_t>();
			}
			if (monitor.contains("framesPerSecond") && monitor["framesPerSecond"].is_number())
			{
				state.framesPerSecond = monitor["framesPerSecond"].get<float>();
			}
			auto entry = CreateMonitor(name, camera["name"].get<std::string>(), state);
			if (!entry || !AddStageMonitor(entry))
			{
				Logger::Log("StageManager: モニターを作れませんでした: " + name + "\n", Logger::LogLevel::Warning);
			}
		}
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
	CaptureSavedState();
	return true;
}

bool StageManager::LoadFromFile()
{
	const std::filesystem::path fullPath = GetFilePath();
	if (!std::filesystem::exists(fullPath))
	{
		Clear();
		CaptureSavedState();
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
	CaptureSavedState();
	return true;
}

bool StageManager::IsDirty() const
{
	// Hierarchy が毎フレーム呼ぶので、JSON の文字列にせず項目を直接比べる（毎フレーム確保しない）
	if (texts3D_.size() != savedTexts_.size()) { return true; }
	for (size_t i = 0; i < texts3D_.size(); ++i)
	{
		const TextEntry& current = texts3D_[i];
		const SavedText& saved = savedTexts_[i];
		if (current.name != saved.name || current.mesh->GetText() != saved.text || !SameSavedParams(current.mesh->GetParams(), saved.params))
		{
			return true;
		}
	}
	if (monitors_.size() != savedMonitors_.size()) { return true; }
	for (size_t i = 0; i < monitors_.size(); ++i)
	{
		const MonitorEntry& current = *monitors_[i];
		const SavedMonitor& saved = savedMonitors_[i];
		if (current.name != saved.name || current.cameraName != saved.cameraName || !SameMonitorState(GetCurrentMonitorState(current), saved.state))
		{
			return true;
		}
	}
	return false;
}

void StageManager::CaptureSavedState()
{
	savedTexts_.clear();
	savedTexts_.reserve(texts3D_.size());
	for (const TextEntry& entry : texts3D_)
	{
		savedTexts_.push_back({ entry.name, entry.mesh->GetText(), entry.mesh->GetParams() });
	}
	savedMonitors_.clear();
	savedMonitors_.reserve(monitors_.size());
	for (const auto& entry : monitors_)
	{
		savedMonitors_.push_back({ entry->name, entry->cameraName, GetCurrentMonitorState(*entry) });
	}
}

#ifdef USE_IMGUI
void StageManager::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterHierarchySection(this, "ステージ", [this]() { DrawHierarchyImGui(); });
	DebugUIManager::GetInstance()->RegisterInspector(this, SelectionKind::Text3D, [this](const SelectionItem& item) { DrawInspectorImGui(item); });
	DebugUIManager::GetInstance()->RegisterInspector(this, SelectionKind::StageMonitor, [this](const SelectionItem& item) { DrawInspectorImGui(item); });
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
	if (ImGui::Button("モニターを作る"))
	{
		newMonitorName_.fill('\0'); createMonitorError_.clear(); createMonitorPopupRequested_ = true;
	}
	const SelectionItem& selected = SelectionContext::GetInstance()->GetPrimary();
	for (const auto& entry : monitors_)
	{
		const bool isSelected = selected.kind == SelectionKind::StageMonitor && selected.name == entry->name;
		if (ImGui::Selectable(entry->name.c_str(), isSelected))
		{
			SelectionItem item; item.kind = SelectionKind::StageMonitor; item.name = entry->name;
			SelectionContext::GetInstance()->Select(item);
		}
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
	if (createMonitorPopupRequested_)
	{
		ImGui::OpenPopup("モニターを作る###CreateStageMonitor");
		createMonitorPopupRequested_ = false;
	}
	if (ImGui::BeginPopupModal("モニターを作る###CreateStageMonitor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("名前", newMonitorName_.data(), newMonitorName_.size());
		if (!createMonitorError_.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.25f, 1.0f), "%s", createMonitorError_.c_str());
		}
		if (ImGui::Button("作る"))
		{
			const std::string name = newMonitorName_.data();
			const std::string cameraName = name + "Cam";
			if (name.empty()) { createMonitorError_ = "名前を入力してください。"; }
			else if (FindMonitor(name)) { createMonitorError_ = "同じ名前のモニターがあります。"; }
			else if (cameraManager_ && cameraManager_->GetCamera(cameraName)) { createMonitorError_ = "同じ名前のカメラがあります: " + cameraName; }
			else if (GameObjectManager::HasInstance() && GameObjectManager::GetInstance()->Find(name)) { createMonitorError_ = "同じ名前の GameObject があります。"; }
			else
			{
				MonitorState state;
				state.screenRotation = kMonitorScreenRotation;
				state.screenScale = kMonitorScreenScale;
				state.width = kDefaultMonitorWidth;
				state.height = kDefaultMonitorHeight;
				state.framesPerSecond = StageMonitor::kDefaultFramesPerSecond;
				if (Camera* camera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr)
				{
					const Matrix4x4& world = camera->GetWorldMatrix();
					const Vector3 forward = { world.m[2][0], world.m[2][1], world.m[2][2] };
					state.screenPosition = camera->GetTranslate() + forward * kCreateDistance;
					state.cameraPosition = camera->GetTranslate();
					state.cameraRotation = camera->GetRotate();
				}
				auto entry = CreateMonitor(name, cameraName, state);
				CommandHistory::GetInstance()->Execute(std::make_unique<CreateStageMonitorCommand>(this, std::move(entry)));
				SelectionItem item; item.kind = SelectionKind::StageMonitor; item.name = name;
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
	if (item.kind == SelectionKind::StageMonitor)
	{
		MonitorEntry* entry = FindMonitor(item.name);
		if (!entry || !entry->screen || !entry->monitor || !entry->monitor->GetCamera()) { return; }
		entry->state = GetCurrentMonitorState(*entry);
		auto finishEdit = [this, entry](const MonitorState& before, bool recreateView)
		{
			if (ImGui::IsItemActivated())
			{
				editingMonitorName_ = entry->name;
				editStartState_ = before;
			}
			if (ImGui::IsItemDeactivatedAfterEdit() && editingMonitorName_ == entry->name)
			{
				CommandHistory::GetInstance()->Execute(std::make_unique<EditStageMonitorCommand>(
					this, entry->name, editStartState_, entry->state, recreateView));
				editingMonitorName_.clear();
			}
		};

		ImGui::SeparatorText("画面");
		MonitorState before = entry->state;
		if (ImGui::DragFloat3("位置", &entry->state.screenPosition.x, kTransformDragSpeed)) { ApplyMonitorState(entry->name, entry->state, false); }
		finishEdit(before, false);
		before = entry->state;
		if (ImGui::DragFloat3("回転", &entry->state.screenRotation.x, kTransformDragSpeed)) { ApplyMonitorState(entry->name, entry->state, false); }
		finishEdit(before, false);
		before = entry->state;
		if (ImGui::DragFloat3("大きさ", &entry->state.screenScale.x, kTransformDragSpeed)) { ApplyMonitorState(entry->name, entry->state, false); }
		finishEdit(before, false);

		ImGui::SeparatorText("カメラ");
		before = entry->state;
		if (ImGui::DragFloat3("カメラ位置", &entry->state.cameraPosition.x, kTransformDragSpeed)) { ApplyMonitorState(entry->name, entry->state, false); }
		finishEdit(before, false);
		before = entry->state;
		if (ImGui::DragFloat3("カメラ回転", &entry->state.cameraRotation.x, kTransformDragSpeed)) { ApplyMonitorState(entry->name, entry->state, false); }
		finishEdit(before, false);

		int resolution[2] = { static_cast<int>(entry->state.width), static_cast<int>(entry->state.height) };
		before = entry->state;
		if (ImGui::DragInt2("解像度", resolution, 1.0f, static_cast<int>(kMinMonitorResolution), static_cast<int>(kMaxMonitorResolution)))
		{
			entry->state.width = static_cast<uint32_t>((std::clamp)(resolution[0], static_cast<int>(kMinMonitorResolution), static_cast<int>(kMaxMonitorResolution)));
			entry->state.height = static_cast<uint32_t>((std::clamp)(resolution[1], static_cast<int>(kMinMonitorResolution), static_cast<int>(kMaxMonitorResolution)));
		}
		finishEdit(before, true);
		before = entry->state;
		if (ImGui::DragFloat("描き直す回数", &entry->state.framesPerSecond, 1.0f, kMinMonitorFramesPerSecond, kMaxMonitorFramesPerSecond, "%.0f fps"))
		{
			ApplyMonitorState(entry->name, entry->state, false);
		}
		finishEdit(before, false);

		ImGui::Separator();
		if (ImGui::Button("ステージから消す"))
		{
			CommandHistory::GetInstance()->Execute(std::make_unique<DeleteStageMonitorCommand>(this, item.name));
		}
		return;
	}
	if (!OwnsText3D(item.name)) { return; }
	ImGui::Separator();
	if (ImGui::Button("ステージから消す"))
	{
		CommandHistory::GetInstance()->Execute(std::make_unique<DeleteStageTextCommand>(this, item.name));
	}
}
#endif
} // namespace KCE
