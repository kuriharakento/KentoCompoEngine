#pragma once
#include <memory>
#include <string>
#include <vector>

#include "editor/command/CompositeCommand.h"
#include "editor/command/ICommand.h"

namespace KCE
{
/** @brief 保持するUndo履歴の既定の上限 */
constexpr size_t kDefaultUndoCapacity = 128;

/**
 * @brief Undo/Redo履歴を管理するクラス
 *
 * @details SEQUENCER_PLAN 5.1。エディタからの全ての編集はここを通す。
 *          タイムラインだけでなく Inspector や Hierarchy からの編集も
 *          同じ履歴に積むことで、一貫したUndoを実現する。
 *
 *          エディタ全体で1つの履歴を共有するためシングルトンとする。
 */
class CommandHistory
{
public:
	static CommandHistory* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 終了処理。履歴を全て破棄する
	 */
	void Finalize();

	/**
	 * @brief コマンドを実行し、Undo履歴に積む
	 *
	 * @details Redo履歴は破棄される。トランザクション中の場合は
	 *          即座に実行したうえで、まとめてのUndo対象として蓄積する。
	 *          直前のコマンドが MergeWith() を受け入れた場合は履歴に積まず統合する。
	 *
	 * @param command 実行するコマンド
	 */
	void Execute(CommandPtr command);

	/**
	 * @brief 直前の操作を取り消す
	 * @return 取り消した場合は真。履歴が空なら偽
	 */
	bool Undo();

	/**
	 * @brief 取り消した操作をやり直す
	 * @return やり直した場合は真。Redo履歴が空なら偽
	 */
	bool Redo();

	/**
	 * @brief トランザクションを開始する
	 *
	 * @details EndTransaction() までに Execute() されたコマンドを
	 *          1つのUndo単位にまとめる。ネストは非対応で、
	 *          既に開始済みの場合は何もしない。
	 *
	 * @param name 履歴に表示する操作名
	 */
	void BeginTransaction(const std::string& name);

	/**
	 * @brief トランザクションを終了し、1つのコマンドとして履歴に積む
	 * @details 1つもコマンドが積まれていない場合、履歴には何も追加しない。
	 */
	void EndTransaction();

	/**
	 * @brief トランザクション中かどうか
	 * @return 開始済みなら真
	 */
	bool IsInTransaction() const { return transaction_ != nullptr; }

	/**
	 * @brief 履歴を全て破棄する
	 * @details シーンの切り替えなど、Undoしても意味が無くなる場面で呼ぶ。
	 */
	void Clear();

	bool CanUndo() const { return !undoStack_.empty(); }
	bool CanRedo() const { return !redoStack_.empty(); }

	/**
	 * @brief 次にUndoされる操作の名前
	 * @return 操作名。Undoできない場合は空文字列
	 */
	std::string GetUndoName() const;

	/**
	 * @brief 次にRedoされる操作の名前
	 * @return 操作名。Redoできない場合は空文字列
	 */
	std::string GetRedoName() const;

	/**
	 * @brief 保持する履歴の上限を設定する
	 * @param capacity 上限数。超えた分は古いものから捨てられる
	 */
	void SetCapacity(size_t capacity);

	size_t GetCapacity() const { return capacity_; }
	size_t GetUndoCount() const { return undoStack_.size(); }

	/**
	 * @brief 最後に保存された時点から変更があるか
	 * @return 変更があれば真
	 */
	bool IsDirty() const { return dirty_; }

	/**
	 * @brief 現在の状態を「保存済み」として記録する
	 */
	void MarkSaved() { dirty_ = false; }

#ifdef USE_IMGUI
	/**
	 * @brief 履歴ウィンドウを描画する
	 */
	void DrawImGui();
#endif

public:
	~CommandHistory() = default;

private:
	static std::unique_ptr<CommandHistory> instance_;
	friend std::unique_ptr<CommandHistory> std::make_unique<CommandHistory>();

	CommandHistory() = default;
	CommandHistory(const CommandHistory&) = delete;
	CommandHistory& operator=(const CommandHistory&) = delete;

	/**
	 * @brief 履歴が上限を超えていたら古いものから捨てる
	 */
	void TrimToCapacity();

	// 実行済みコマンド。末尾が直近の操作
	std::vector<CommandPtr> undoStack_;
	// Undoされたコマンド。末尾が次にRedoされる操作
	std::vector<CommandPtr> redoStack_;
	// トランザクション中に蓄積されるコマンド
	std::unique_ptr<CompositeCommand> transaction_;
	// 保持する履歴の上限
	size_t capacity_ = kDefaultUndoCapacity;
	// 最後の保存以降に変更があるか
	bool dirty_ = false;
};
} // namespace KCE
