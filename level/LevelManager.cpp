#include "level/LevelManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "manager/editor/DebugUIManager.h"
#include "level/CameraElement.h"
#include "level/ModelElement.h"
#include "level/SpriteElement.h"
#include "level/MonitorElement.h"
#include "level/TextElement.h"
#include "time/TimeManager.h"
#ifdef USE_IMGUI
#include "base/PathManager.h"
#include "editor/SceneGizmo.h"
#include "editor/SelectionContext.h"
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
std::unique_ptr<LevelManager> LevelManager::instance_ = nullptr;

LevelManager* LevelManager::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<LevelManager>();
	}
	return instance_.get();
}

bool LevelManager::HasInstance()
{
	return instance_ != nullptr;
}

void LevelManager::Initialize(const SceneContext& context)
{
	context_ = context;
	// 参照される種類を先に、参照する種類を後に登録する
	RegisterElementType(ModelElement::MakeType());
	RegisterElementType(TextElement::MakeType());
	RegisterElementType(SpriteElement::MakeType());
	// カメラは、モニターなど映す側より先に登録する
	RegisterElementType(CameraElement::MakeType());
	// モニターは、映すカメラより後に登録する
	RegisterElementType(MonitorElement::MakeType());
#ifdef USE_IMGUI
	// 初めて開くときは、Hierarchy と同じ左側のタブに入る
	DebugUIManager::GetInstance()->RegisterWindow(this, "Level###Level", [this]() { DrawWindowImGui(); }, EditorDock::Left);
	// 要素ごとの詳細とギズモは、選ばれた物の持ち主に渡す（持ち主でなければ要素が何もしない）
	for (const SelectionKind kind : { SelectionKind::Text3D, SelectionKind::Camera, SelectionKind::StageMonitor, SelectionKind::Sprite2D })
	{
		DebugUIManager::GetInstance()->RegisterInspector(this, kind, [this](const SelectionItem& item)
		{
			for (auto& [name, stage] : stages_)
			{
				for (const auto& element : stage->GetElements()) { element->DrawInspector(item); }
			}
		});
		GizmoTarget target;
		target.getPose = [this](const SelectionItem& item, Matrix4x4& world, uint32_t& operations)
		{
			for (auto& [name, stage] : stages_)
			{
				for (const auto& element : stage->GetElements())
				{
					if (element->GetGizmoPose(item, world, operations)) { return true; }
				}
			}
			return false;
		};
		target.apply = [this](const SelectionItem& item, const GizmoResult& result, uint32_t dragId)
		{
			for (auto& [name, stage] : stages_)
			{
				for (const auto& element : stage->GetElements()) { element->ApplyGizmo(item, result, dragId); }
			}
		};
		SceneGizmo::GetInstance()->RegisterTarget(this, kind, std::move(target));
	}
#endif
}

void LevelManager::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
	if (SceneGizmo::HasInstance())
	{
		SceneGizmo::GetInstance()->Unregister(this);
	}
#endif
	pendingUnloads_.clear();
	stages_.clear();
	types_.clear();
	instance_.reset();
}

void LevelManager::RegisterElementType(LevelElementType type)
{
	const auto found = std::find_if(types_.begin(), types_.end(), [&type](const LevelElementType& registered) { return registered.key == type.key; });
	if (found != types_.end())
	{
		Logger::Log("LevelManager: 同じ種類を二重に登録しました: " + type.key + "\n", Logger::LogLevel::Warning);
		return;
	}
	types_.push_back(std::move(type));
}

Level* LevelManager::Load(const std::string& name)
{
	if (name.empty())
	{
		return nullptr;
	}
	if (Level* loaded = Find(name))
	{
		return loaded;
	}
	auto stage = std::make_unique<Level>(name, context_);
	if (!stage->Load())
	{
		return nullptr;
	}
	Level* result = stage.get();
	stages_[name] = std::move(stage);
	return result;
}

void LevelManager::Unload(const std::string& name)
{
	if (Find(name) && std::find(pendingUnloads_.begin(), pendingUnloads_.end(), name) == pendingUnloads_.end())
	{
		pendingUnloads_.push_back(name);
	}
}

void LevelManager::ProcessPendingUnloads(DirectXCommon* dxCommon)
{
	if (pendingUnloads_.empty()) { return; }
	auto names = std::move(pendingUnloads_);
	pendingUnloads_.clear();
	// 前のフレームまでの描画が GPU で終わってから壊す
	if (dxCommon) { dxCommon->ExecuteAndWait(); }
	for (const std::string& name : names)
	{
		stages_.erase(name);
	}
}

Level* LevelManager::Find(const std::string& name) const
{
	const auto found = stages_.find(name);
	return found != stages_.end() ? found->second.get() : nullptr;
}

void LevelManager::Update()
{
	const float deltaTime = TimeManager::GetInstance().GetDeltaTime(GameClock());
	for (auto& [name, stage] : stages_)
	{
		stage->SetSequencerState(sequencerActive_, sequencerPlaying_);
		stage->Update(deltaTime);
	}
}

#ifdef USE_IMGUI
namespace
{
/** @brief 大文字と小文字を区別せず、text が needle を含むか。needle が空なら真 */
bool ContainsIgnoreCase(const std::string& text, const char* needle)
{
	const std::string target(needle);
	const auto found = std::search(text.begin(), text.end(), target.begin(), target.end(),
		[](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
	return target.empty() || found != text.end();
}

/** @brief Level 名にしてよいか。ファイル名になるので、パスや使えない文字を含めない */
bool IsValidStageName(const std::string& name)
{
	if (name.empty() || name.front() == ' ' || name.back() == ' ' || name.find("..") != std::string::npos)
	{
		return false;
	}
	return name.find_first_of("\\/:*?\"<>|") == std::string::npos;
}

/** @brief 指定フォルダの .json を並べる。stripExtension なら拡張子を落とす */
std::vector<std::string> ListFiles(const std::filesystem::path& directory, bool stripExtension)
{
	std::vector<std::string> files;
	std::error_code error;
	if (!std::filesystem::is_directory(directory, error))
	{
		return files;
	}
	for (const auto& entry : std::filesystem::directory_iterator(directory, error))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".json")
		{
			files.push_back(stripExtension ? entry.path().stem().string() : entry.path().filename().string());
		}
	}
	std::sort(files.begin(), files.end());
	return files;
}
} // namespace

std::vector<std::string> LevelManager::ListStageFiles()
{
	return ListFiles(PathManager::GetApplicationResourceRoot() / "json" / "level", true);
}

std::vector<std::string> LevelManager::ListPrefabFiles()
{
	return ListFiles(PathManager::GetApplicationResourceRoot() / "json" / "prefab", false);
}

bool LevelManager::DuplicateElement(Level& stage, const std::string& name)
{
	const ILevelElement* source = stage.Find<ILevelElement>(name);
	if (!source)
	{
		return false;
	}
	const std::string typeKey = source->GetTypeKey();
	const auto type = std::find_if(types_.begin(), types_.end(), [&typeKey](const LevelElementType& t) { return t.key == typeKey; });
	if (type == types_.end() || !type->create)
	{
		return false;
	}
	nlohmann::json entry = source->Serialize();
	entry["name"] = MakeUniqueName(stage, name);
	std::unique_ptr<ILevelElement> copy;
	try
	{
		copy = type->create(entry, context_);
	}
	catch (const nlohmann::json::exception&)
	{
		return false;
	}
	return copy && stage.Add(copy);
}

std::string LevelManager::MakeUniqueName(const Level& stage, const std::string& baseName)
{
	if (!stage.Find<ILevelElement>(baseName))
	{
		return baseName;
	}
	for (int suffix = 2;; ++suffix)
	{
		const std::string candidate = baseName + "_" + std::to_string(suffix);
		if (!stage.Find<ILevelElement>(candidate))
		{
			return candidate;
		}
	}
}

void LevelManager::DrawWindowImGui()
{
	DrawStagePickerImGui();
	ImGui::Separator();
	Level* stage = Find(selectedStageName_);
	if (!stage)
	{
		ImGui::TextDisabled("Level を読み込むか、新規作成してください。");
		return;
	}
	DrawStageBodyImGui(*stage);
}

void LevelManager::DrawStagePickerImGui()
{
	// 選んでいた Level が無くなっていたら、残っている先頭のものに合わせる
	if (!Find(selectedStageName_))
	{
		selectedStageName_ = stages_.empty() ? std::string() : stages_.begin()->first;
	}
	const ImGuiStyle& style = ImGui::GetStyle();
	// 右に置くボタンの分を残して、入力欄とコンボの幅を決める
	const auto widthBeside = [&style](const char* buttonLabel)
	{
		return ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(buttonLabel).x - style.FramePadding.x * 2.0f - style.ItemSpacing.x;
	};

	// 読み込み済みの Level を切り替える
	const char* preview = selectedStageName_.empty() ? "（なし）" : selectedStageName_.c_str();
	ImGui::SetNextItemWidth(widthBeside("閉じる"));
	if (ImGui::BeginCombo("##LoadedStage", preview))
	{
		for (const auto& [name, stage] : stages_)
		{
			if (ImGui::Selectable(name.c_str(), name == selectedStageName_))
			{
				selectedStageName_ = name;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(selectedStageName_.empty());
	if (ImGui::Button("閉じる"))
	{
		const Level* stage = Find(selectedStageName_);
		if (stage && stage->IsDirty())
		{
			// 保存していない変更があるときは、確認してから閉じる
			ImGui::OpenPopup("ConfirmCloseStage");
		}
		else
		{
			Unload(selectedStageName_);
		}
	}
	ImGui::EndDisabled();
	if (ImGui::BeginPopupModal("ConfirmCloseStage", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("「%s」に保存していない変更があります。", selectedStageName_.c_str());
		if (ImGui::Button("保存して閉じる"))
		{
			if (Level* stage = Find(selectedStageName_); stage && stage->Save())
			{
				Unload(selectedStageName_);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("保存せず閉じる"))
		{
			Unload(selectedStageName_);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// フォルダの中の Level を選んで読み込む
	if (ImGui::CollapsingHeader("ファイルから読み込む"))
	{
		ImGui::BeginChild("StageFiles", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 6.0f), true);
		const std::vector<std::string> files = ListStageFiles();
		if (files.empty())
		{
			ImGui::TextDisabled("Resources/json/level に Level のファイルがない。");
		}
		for (const std::string& file : files)
		{
			const bool loaded = Find(file) != nullptr;
			const std::string label = loaded ? file + "（読み込み済み）" : file;
			ImGui::PushID(file.c_str());
			if (ImGui::Selectable(label.c_str(), file == selectedStageName_))
			{
				if (loaded || Load(file))
				{
					selectedStageName_ = file;
					loadError_.clear();
				}
				else
				{
					loadError_ = "読み込めませんでした: " + file;
				}
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	// 新しいステージを作る。同じ名前のファイルがあれば、それを読み込む
	ImGui::SetNextItemWidth(widthBeside("作成"));
	ImGui::InputTextWithHint("##NewStageName", "新しいステージ名", newStageName_.data(), newStageName_.size());
	ImGui::SameLine();
	if (ImGui::Button("作成"))
	{
		const std::string name = newStageName_.data();
		if (!IsValidStageName(name))
		{
			loadError_ = "Level 名が空か、使えない文字が入っています";
		}
		else if (Load(name))
		{
			selectedStageName_ = name;
			newStageName_.fill('\0');
			loadError_.clear();
		}
		else
		{
			loadError_ = "作れませんでした: " + name;
		}
	}
	if (!loadError_.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", loadError_.c_str());
	}
}

void LevelManager::DrawStageBodyImGui(Level& stage)
{
	if (stage.IsDirty())
	{
		ImGui::TextDisabled("保存していない変更があります");
	}
	if (ImGui::Button(stage.IsPlaying() ? "停止（編集）" : "再生"))
	{
		stage.SetPlaying(!stage.IsPlaying());
	}
	ImGui::SameLine();
	if (ImGui::Button("保存"))
	{
		stage.Save();
	}

	// 追加のメニュー。ボタンからも、一覧の空いている所の右クリックからも開く
	const auto drawAddMenuItems = [this, &stage]()
	{
		// 編集の邪魔にならないよう、再生中は足せないようにする
		ImGui::BeginDisabled(stage.IsPlaying());
		// 種類ごとの「空の新規」。登録した種類が増えれば、自動で並ぶ
		for (const LevelElementType& type : types_)
		{
			if (!type.createDefault)
			{
				continue;
			}
			const std::string label = type.displayName + "を追加";
			if (ImGui::MenuItem(label.c_str()))
			{
				auto element = type.createDefault(MakeUniqueName(stage, type.defaultName.empty() ? type.key : type.defaultName), context_);
				stage.Add(element);
			}
		}
		ImGui::Separator();
		for (const std::string& file : ListPrefabFiles())
		{
			if (ImGui::MenuItem(file.c_str()))
			{
				auto model = ModelElement::CreateFromPrefab(MakeUniqueName(stage, std::filesystem::path(file).stem().string()), file);
				if (model)
				{
					std::unique_ptr<ILevelElement> element = std::move(model);
					stage.Add(element);
				}
			}
		}
		ImGui::EndDisabled();
	};
	ImGui::SameLine();
	if (ImGui::Button("モデルを追加"))
	{
		ImGui::OpenPopup("AddModelPopup");
	}
	if (ImGui::BeginPopup("AddModelPopup"))
	{
		drawAddMenuItems();
		ImGui::EndPopup();
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##ElementFilter", "検索", elementFilter_.data(), elementFilter_.size());
	ImGui::BeginChild("StageElements", ImVec2(0.0f, 0.0f), true);
	std::string removeName;
	std::string duplicateName;
	for (const auto& element : stage.GetElements())
	{
		if (!ContainsIgnoreCase(element->GetName(), elementFilter_.data()))
		{
			continue;
		}
		element->DrawHierarchyRow();
		ImGui::PushID(element.get());
		if (ImGui::BeginPopupContextItem("StageElementMenu"))
		{
			ImGui::BeginDisabled(stage.IsPlaying());
			if (ImGui::MenuItem("名前を変更"))
			{
				renameTarget_ = element->GetName();
				renameBuffer_.fill(0);
				renameTarget_.copy(renameBuffer_.data(), renameBuffer_.size() - 1);
				renameError_.clear();
				renamePopupRequested_ = true;
			}
			if (ImGui::MenuItem("複製"))
			{
				duplicateName = element->GetName();
			}
			if (ImGui::MenuItem("削除"))
			{
				removeName = element->GetName();
			}
			ImGui::EndDisabled();
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}
	// 一覧の空いている所で右クリックしても足せる
	if (ImGui::BeginPopupContextWindow("StageListMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		drawAddMenuItems();
		ImGui::EndPopup();
	}
	// 描き終えてから外す。描画中に並びを変えない
	if (!removeName.empty())
	{
		stage.Remove(removeName);
	}
	if (!duplicateName.empty())
	{
		DuplicateElement(stage, duplicateName);
	}
	ImGui::EndChild();

	// 名前の変更。メニューの中では開けないので、フラグを受けてここで開く
	if (renamePopupRequested_)
	{
		renamePopupRequested_ = false;
		ImGui::OpenPopup("RenameElement");
	}
	if (ImGui::BeginPopupModal("RenameElement", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("「%s」の新しい名前", renameTarget_.c_str());
		if (ImGui::IsWindowAppearing()) { ImGui::SetKeyboardFocusHere(); }
		const bool submitted = ImGui::InputText("##RenameInput", renameBuffer_.data(), renameBuffer_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
		const bool pressed = ImGui::Button("変更");
		ImGui::SameLine();
		const bool cancelled = ImGui::Button("キャンセル");
		if (submitted || pressed)
		{
			if (stage.Rename(renameTarget_, renameBuffer_.data()))
			{
				ImGui::CloseCurrentPopup();
			}
			else
			{
				renameError_ = "変更できません（名前が空か、すでに使われています）";
			}
		}
		if (cancelled) { ImGui::CloseCurrentPopup(); }
		if (!renameError_.empty()) { ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", renameError_.c_str()); }
		ImGui::EndPopup();
	}
}
#endif
} // namespace KCE
