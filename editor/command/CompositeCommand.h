#pragma once
#include <string>
#include <utility>
#include <vector>

#include "editor/command/ICommand.h"

namespace KCE
{
/**
 * @brief 複数のコマンドを1つのUndo単位にまとめるコマンド
 * @details 「選択中の全キーを削除」のような一括操作で使う。
 *          Undoは追加した逆順に実行され、操作前の状態を正しく復元する。
 */
class CompositeCommand : public ICommand
{
public:
	/**
	 * @brief コンストラクタ
	 * @param name 履歴に表示する操作名
	 */
	explicit CompositeCommand(std::string name) : name_(std::move(name)) {}

	/**
	 * @brief 子コマンドを追加する
	 * @details 追加した順に Execute される。追加時点では実行しない。
	 * @param command 追加するコマンド
	 */
	void Add(CommandPtr command)
	{
		if (command) { commands_.push_back(std::move(command)); }
	}

	/**
	 * @brief 子コマンドを持たないか
	 * @return 1つも持たなければ真
	 */
	bool IsEmpty() const { return commands_.empty(); }

	/**
	 * @brief 子コマンドの数
	 * @return コマンド数
	 */
	size_t GetCount() const { return commands_.size(); }

	void Execute() override
	{
		for (auto& command : commands_) { command->Execute(); }
	}

	void Undo() override
	{
		// 依存関係を壊さないよう、実行した逆順に戻す
		for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) { (*it)->Undo(); }
	}

	std::string GetName() const override { return name_; }

private:
	// 履歴に表示する操作名
	std::string name_;
	// 追加順に保持された子コマンド
	std::vector<CommandPtr> commands_;
};
} // namespace KCE
