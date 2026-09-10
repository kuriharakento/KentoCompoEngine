#include "graphics/shader/ShaderHotReload.h"

#include <algorithm>

#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "time/TimeManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
/** @brief ファイルの更新確認を行う間隔（秒） */
constexpr float kCheckInterval = 0.5f;

/** @brief 監視対象とするシェーダーの拡張子 */
bool IsShaderFile(const std::filesystem::path& path)
{
	const std::wstring extension = path.extension().wstring();
	return extension == L".hlsl" || extension == L".hlsli";
}
} // namespace

std::unique_ptr<ShaderHotReload> ShaderHotReload::instance_ = nullptr;

ShaderHotReload* ShaderHotReload::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<ShaderHotReload>();
	}
	return instance_.get();
}

bool ShaderHotReload::HasInstance()
{
	return instance_ != nullptr;
}

void ShaderHotReload::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;

	// 作業ディレクトリが実行環境によって異なるため、候補をすべて登録しておく。
	// 存在しないものは AddWatchDirectory 側で弾かれる。
	AddWatchDirectory("engine/Resources/shaders");
	AddWatchDirectory("Resources/shaders");
	AddWatchDirectory("../Resources/shaders");

	lastWriteTime_ = GetLatestWriteTime();
	hasWriteTime_ = true;
}

void ShaderHotReload::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
	targets_.clear();
	watchDirectories_.clear();
	dxCommon_ = nullptr;
	instance_.reset();
}

void ShaderHotReload::AddWatchDirectory(const std::filesystem::path& directory)
{
	std::error_code ec;
	if (!std::filesystem::exists(directory, ec) || !std::filesystem::is_directory(directory, ec))
	{
		return;
	}

	const auto it = std::find(watchDirectories_.begin(), watchDirectories_.end(), directory);
	if (it != watchDirectories_.end())
	{
		return;
	}

	watchDirectories_.push_back(directory);
}

void ShaderHotReload::Register(void* owner, const std::string& name, ShaderReloadCallback rebuild)
{
	if (!owner || !rebuild)
	{
		return;
	}

	ShaderReloadTarget target;
	target.owner = owner;
	target.name = name;
	target.rebuild = std::move(rebuild);
	targets_.push_back(std::move(target));
}

void ShaderHotReload::Unregister(void* owner)
{
	targets_.erase(
		std::remove_if(targets_.begin(), targets_.end(),
			[owner](const ShaderReloadTarget& target) { return target.owner == owner; }),
		targets_.end());
}

std::filesystem::file_time_type ShaderHotReload::GetLatestWriteTime() const
{
	std::filesystem::file_time_type latest{};

	for (const auto& directory : watchDirectories_)
	{
		std::error_code ec;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, ec))
		{
			if (ec)
			{
				break;
			}
			if (!entry.is_regular_file() || !IsShaderFile(entry.path()))
			{
				continue;
			}

			const auto writeTime = std::filesystem::last_write_time(entry.path(), ec);
			if (!ec && writeTime > latest)
			{
				latest = writeTime;
			}
		}
	}

	return latest;
}

void ShaderHotReload::Update()
{
	if (!autoReloadEnabled_ || targets_.empty() || watchDirectories_.empty())
	{
		return;
	}

	// 毎フレーム全ファイルの更新時刻を見るのはディスクアクセスが無駄なので間引く。
	// 編集モードではゲーム時間が止まるため、実時間の差分を使う。
	checkTimer_ += TimeManager::GetInstance().GetGameContext().realDeltaTime;
	if (checkTimer_ < kCheckInterval)
	{
		return;
	}
	checkTimer_ = 0.0f;

	const auto latest = GetLatestWriteTime();
	if (!hasWriteTime_)
	{
		lastWriteTime_ = latest;
		hasWriteTime_ = true;
		return;
	}

	if (latest <= lastWriteTime_)
	{
		return;
	}

	lastWriteTime_ = latest;
	ReloadAll();
}

bool ShaderHotReload::ReloadAll()
{
	if (targets_.empty())
	{
		statusMessage_ = "リロード対象がありません";
		return true;
	}

	// 実行中のコマンドリストが参照しているPSOを解放しないよう、
	// 差し替える前にGPUの完了を待つ。
	if (dxCommon_)
	{
		dxCommon_->ExecuteAndWait();
	}

	int successCount = 0;
	int failureCount = 0;

	for (auto& target : targets_)
	{
		std::string error;
		const bool succeeded = target.rebuild(error);

		target.lastSucceeded = succeeded;
		target.lastError = succeeded ? std::string() : error;

		if (succeeded)
		{
			++successCount;
		}
		else
		{
			++failureCount;
			// 失敗しても既存のPSOはそのまま使われ続ける。
			// 直せばもう一度保存するだけでリロードされる。
			Logger::Log("ShaderHotReload: " + target.name + " のリロードに失敗しました\n" + error + "\n", Logger::LogLevel::Error);
		}
	}

	statusMessage_ = "成功 " + std::to_string(successCount) + " / 失敗 " + std::to_string(failureCount);
	Logger::Log("ShaderHotReload: " + statusMessage_ + "\n",
		failureCount > 0 ? Logger::LogLevel::Warning : Logger::LogLevel::Info);

	return failureCount == 0;
}

#ifdef USE_IMGUI
void ShaderHotReload::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterDebugUI(
		this, "Shader Hot Reload", [this]() { this->DrawImGui(); }, DebugUIArea::Console);
}

void ShaderHotReload::DrawImGui()
{
	if (ImGui::Button("Reload Now"))
	{
		ReloadAll();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Auto Reload", &autoReloadEnabled_);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("シェーダーファイルの更新を %.1f 秒ごとに確認します", kCheckInterval);
	}

	if (!statusMessage_.empty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("%s", statusMessage_.c_str());
	}

	ImGui::Separator();

	ImGui::TextDisabled("監視中のディレクトリ");
	if (watchDirectories_.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "  見つかりません");
	}
	for (const auto& directory : watchDirectories_)
	{
		ImGui::BulletText("%s", directory.string().c_str());
	}

	ImGui::Separator();

	ImGui::TextDisabled("リロード対象 (%zu)", targets_.size());
	for (const auto& target : targets_)
	{
		if (target.lastSucceeded)
		{
			ImGui::BulletText("%s", target.name.c_str());
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
			const bool opened = ImGui::TreeNode(target.name.c_str(), "%s (失敗)", target.name.c_str());
			ImGui::PopStyleColor();
			if (opened)
			{
				ImGui::TextWrapped("%s", target.lastError.c_str());
				ImGui::TreePop();
			}
		}
	}
}
#endif
} // namespace KCE
