#pragma once
#include <string>

#include <nlohmann/json.hpp>

#include "editor/command/ICommand.h"
#include "sequencer/core/Sequence.h"

namespace KCE
{
/**
 * @brief 1トラックへの編集を、編集前後のスナップショットで表すコマンド
 *
 * @details キーの追加・移動・削除・ベジェハンドルの変更を、すべてこの1種類で扱う。
 *          操作ごとに専用コマンドを書くより取り違えが起きにくく、
 *          Undo が「編集前のJSONを流し込むだけ」になるため常に正しい。
 *
 *          トラックはポインタではなくインデックスで指す。ポインタで持つと、
 *          Undoでトラックを消したあとに実体が消えてダングリングするため。
 *
 *          @b 使い方
 *          @code
 *          // 1. 編集前のスナップショットを取る
 *          auto command = std::make_unique<TrackEditCommand>(sequence, trackIndex, "Add Camera Key");
 *          // 2. 実際にトラックを編集する
 *          cameraTrack->AddKeyFromCamera(time, camera);
 *          // 3. 編集後のスナップショットを取って履歴へ積む
 *          command->CaptureAfter();
 *          CommandHistory::GetInstance()->Execute(std::move(command));
 *          @endcode
 */
class TrackEditCommand : public ICommand
{
public:
	/**
	 * @brief コンストラクタ。この時点で編集前の状態を記録する
	 * @param sequence 対象のシーケンス
	 * @param trackIndex 対象トラックのインデックス
	 * @param name 履歴に表示する操作名
	 */
	TrackEditCommand(Sequence* sequence, size_t trackIndex, std::string name);

	/**
	 * @brief 編集後の状態を記録する
	 * @details トラックを実際に編集したあと、履歴へ積む前に必ず呼ぶこと。
	 */
	void CaptureAfter();

	/**
	 * @brief 編集前後で内容が変わったか
	 * @details 変化が無いなら履歴に積む必要はない。
	 * @return 変化があれば真
	 */
	bool HasChanged() const;

	void Execute() override;
	void Undo() override;
	std::string GetName() const override { return name_; }

	/**
	 * @brief ドラッグ中の連続編集を1つにまとめる
	 * @details 同じシーケンスの同じトラックに対する同名の操作なら、
	 *          編集後の状態だけを引き継いで統合する。
	 * @param next 後続のコマンド
	 * @return 統合したら真
	 */
	bool MergeWith(const ICommand* next) override;

private:
	Sequence* sequence_ = nullptr;
	size_t trackIndex_ = 0;
	std::string name_;
	nlohmann::json before_;
	nlohmann::json after_;
	bool hasAfter_ = false;
};

/**
 * @brief トラックの追加・削除など、シーケンスの構造を変えるコマンド
 * @details 構造変更はキー編集に比べて頻度が低いため、
 *          シーケンス全体のスナップショットで扱う。
 */
class SequenceStructureCommand : public ICommand
{
public:
	/**
	 * @brief コンストラクタ。この時点でシーケンス全体を記録する
	 * @param sequence 対象のシーケンス
	 * @param name 履歴に表示する操作名
	 */
	SequenceStructureCommand(Sequence* sequence, std::string name);

	/**
	 * @brief 編集後の状態を記録する
	 */
	void CaptureAfter();

	/**
	 * @brief 編集前後で内容が変わったか
	 * @return 変化があれば真
	 */
	bool HasChanged() const;

	void Execute() override;
	void Undo() override;
	std::string GetName() const override { return name_; }

private:
	Sequence* sequence_ = nullptr;
	std::string name_;
	nlohmann::json before_;
	nlohmann::json after_;
	bool hasAfter_ = false;
};
} // namespace KCE
