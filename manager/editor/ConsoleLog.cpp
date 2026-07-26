#include "ConsoleLog.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"

#endif

namespace KCE
{

// 最大蓄積行数
constexpr size_t kMaxLogSize = 1000;

std::unique_ptr<ConsoleLog> ConsoleLog::instance_ = nullptr;

ConsoleLog* ConsoleLog::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_.reset(new ConsoleLog());
	}
	return instance_.get();
}

bool ConsoleLog::HasInstance()
{
	return instance_ != nullptr;
}

void ConsoleLog::Initialize()
{
#ifdef USE_IMGUI
	std::lock_guard<std::mutex> lock(mutex_);
	logs_.clear();
#endif
}

void ConsoleLog::Finalize()
{
#ifdef USE_IMGUI
	{
		std::lock_guard<std::mutex> lock(mutex_);
		logs_.clear();
	}
#endif
	instance_.reset();
}

void ConsoleLog::AddLog([[maybe_unused]] const std::string& message, [[maybe_unused]] Logger::LogLevel level)
{
#ifdef USE_IMGUI
	std::lock_guard<std::mutex> lock(mutex_);

	logs_.push_back({ message, level });

	// 最大行数を超えたら古いものを削除
	if (logs_.size() > kMaxLogSize)
	{
		logs_.erase(logs_.begin());
	}
#endif
}

void ConsoleLog::Clear()
{
#ifdef USE_IMGUI
	std::lock_guard<std::mutex> lock(mutex_);
	logs_.clear();
#endif
}

void ConsoleLog::Draw([[maybe_unused]] bool* open)
{
#ifdef USE_IMGUI
	if (open && !*open)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console", open))
	{
		ImGui::End();
		return;
	}

	// ツールバー（ログクリアなど）
	if (ImGui::Button("Clear"))
	{
		Clear();
	}
	ImGui::SameLine();
	bool scroll_to_bottom = false;
	if (ImGui::Button("Scroll to Bottom"))
	{
		scroll_to_bottom = true;
	}

	ImGui::Separator();

	// スクロール可能な子ウィンドウ領域
	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (const auto& log : logs_)
		{
			ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default Info = White

			if (log.level == Logger::LogLevel::Warning)
			{
				color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Yellow
			}
			else if (log.level == Logger::LogLevel::Error)
			{
				color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Red
			}

			ImGui::TextColored(color, "%s", log.message.c_str());
		}
	}

	if (scroll_to_bottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
	ImGui::End();
#endif
}
} // namespace KCE
