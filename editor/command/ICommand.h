#pragma once
#include <memory>
#include <string>

namespace KCE
{
/**
 * @brief エディタの編集操作を表すコマンド
 *
 * @details SEQUENCER_PLAN 5.1 の規約。エディタからの値の書き換えは、
 *          直接代入せず必ずこのコマンド経由で行う。
 *
 *          コマンドの粒度は「ユーザーの1操作 ＝ 1コマンド」とする。
 *          - ドラッグ操作は、掴んでから離すまでで1コマンド。
 *            毎フレーム発行して MergeWith() でまとめるか、離した時点で
 *            開始値と終了値から1つ発行するかは実装側の判断でよい。
 *          - 複数オブジェクトへの一括操作は CommandHistory の
 *            トランザクションでまとめ、Undo1回で全て戻るようにする。
 */
class ICommand
{
public:
	virtual ~ICommand() = default;

	/**
	 * @brief 操作を適用する
	 * @details CommandHistory::Execute() から最初に1回呼ばれ、
	 *          以降 Redo のたびに呼ばれる。何度呼んでも同じ結果になること。
	 */
	virtual void Execute() = 0;

	/**
	 * @brief 操作を取り消し、実行前の状態に戻す
	 */
	virtual void Undo() = 0;

	/**
	 * @brief Undo履歴に表示する操作名
	 * @return 操作名（例: "Move Camera Key"）
	 */
	virtual std::string GetName() const = 0;

	/**
	 * @brief 直前のコマンドに自身を統合できるか試みる
	 *
	 * @details ドラッグ中に毎フレーム発行されるような連続操作を、
	 *          履歴上1つにまとめるために使う。
	 *          統合した場合、呼ばれた側（履歴に既にある方）が
	 *          「開始状態は自分のもの・終了状態は引数のもの」を持つように更新する。
	 *
	 * @param next 後から発行されたコマンド
	 * @return 統合した場合は真。偽の場合、next は別コマンドとして積まれる
	 */
	virtual bool MergeWith(const ICommand* next) { (void)next; return false; }
};

using CommandPtr = std::unique_ptr<ICommand>;
} // namespace KCE
