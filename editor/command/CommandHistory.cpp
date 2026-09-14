#include "editor/command/CommandHistory.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KCE
{
std::unique_ptr<CommandHistory> CommandHistory::instance_ = nullptr;

CommandHistory* CommandHistory::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<CommandHistory>();
	}
	return instance_.get();
}

bool CommandHistory::HasInstance()
{
	return instance_ != nullptr;
}

void CommandHistory::Finalize()
{
	Clear();
	instance_.reset();
}

void CommandHistory::Execute(CommandPtr command)
{
	if (!command)
	{
		return;
	}

	command->Execute();
	dirty_ = true;

	// トランザクション中はまとめ先に溜める
	if (transaction_)
	{
		transaction_->Add(std::move(command));
		return;
	}

	// 新しい操作を行ったのでRedo履歴は無効になる
	redoStack_.clear();

	// ドラッグ中の連続操作は直前のコマンドに統合する
	if (!undoStack_.empty() && undoStack_.back()->MergeWith(command.get()))
	{
		return;
	}

	undoStack_.push_back(std::move(command));
	TrimToCapacity();
}

bool CommandHistory::Undo()
{
	// トランザクションが開きっぱなしの状態でのUndoは履歴が壊れるため、先に閉じる
	if (transaction_)
	{
		EndTransaction();
	}

	if (undoStack_.empty())
	{
		return false;
	}

	CommandPtr command = std::move(undoStack_.back());
	undoStack_.pop_back();
	command->Undo();
	redoStack_.push_back(std::move(command));
	dirty_ = true;
	return true;
}

bool CommandHistory::Redo()
{
	if (transaction_)
	{
		EndTransaction();
	}

	if (redoStack_.empty())
	{
		return false;
	}

	CommandPtr command = std::move(redoStack_.back());
	redoStack_.pop_back();
	command->Execute();
	undoStack_.push_back(std::move(command));
	dirty_ = true;
	return true;
}

void CommandHistory::BeginTransaction(const std::string& name)
{
	// ネストは扱わない。開始済みなら最初の開始を優先する
	if (transaction_)
	{
		return;
	}
	transaction_ = std::make_unique<CompositeCommand>(name);
}

void CommandHistory::EndTransaction()
{
	if (!transaction_)
	{
		return;
	}

	std::unique_ptr<CompositeCommand> transaction = std::move(transaction_);
	transaction_ = nullptr;

	// 何も行われなかったトランザクションは履歴に残さない
	if (transaction->IsEmpty())
	{
		return;
	}

	redoStack_.clear();
	undoStack_.push_back(std::move(transaction));
	TrimToCapacity();
}

void CommandHistory::Clear()
{
	undoStack_.clear();
	redoStack_.clear();
	transaction_.reset();
	dirty_ = false;
}

std::string CommandHistory::GetUndoName() const
{
	return undoStack_.empty() ? std::string() : undoStack_.back()->GetName();
}

std::string CommandHistory::GetRedoName() const
{
	return redoStack_.empty() ? std::string() : redoStack_.back()->GetName();
}

void CommandHistory::SetCapacity(size_t capacity)
{
	capacity_ = capacity == 0 ? 1 : capacity;
	TrimToCapacity();
}

void CommandHistory::TrimToCapacity()
{
	if (undoStack_.size() <= capacity_)
	{
		return;
	}

	const size_t excess = undoStack_.size() - capacity_;
	undoStack_.erase(undoStack_.begin(), undoStack_.begin() + excess);
}

#ifdef USE_IMGUI
void CommandHistory::DrawImGui()
{
	ImGui::Text("Undo: %zu / %zu", undoStack_.size(), capacity_);
	ImGui::SameLine();
	ImGui::TextDisabled("Redo: %zu", redoStack_.size());

	ImGui::BeginDisabled(!CanUndo());
	if (ImGui::Button("Undo"))
	{
		Undo();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	ImGui::BeginDisabled(!CanRedo());
	if (ImGui::Button("Redo"))
	{
		Redo();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("履歴を消す"))
	{
		Clear();
	}

	if (dirty_)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "*unsaved");
	}

	ImGui::Separator();

	if (ImGui::BeginChild("##CommandHistoryList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
	{
		// 直近の操作が上に来るように逆順で並べる
		for (auto it = undoStack_.rbegin(); it != undoStack_.rend(); ++it)
		{
			const bool isLatest = (it == undoStack_.rbegin());
			if (isLatest)
			{
				ImGui::BulletText("%s", (*it)->GetName().c_str());
			}
			else
			{
				ImGui::TextDisabled("   %s", (*it)->GetName().c_str());
			}
		}

		for (auto it = redoStack_.rbegin(); it != redoStack_.rend(); ++it)
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "   (redo) %s", (*it)->GetName().c_str());
		}
	}
	ImGui::EndChild();
}
#endif
} // namespace KCE
