#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace KCE
{
class GameObject;

/**
 * @brief ゲームの出来事（イベント名）から、再生するシーケンスを引く表
 *
 * JSON（application/Resources/json/sequence_triggers.json）の例:
 *   { "triggers": [
 *     { "event": "PartStarted", "sequence": "part_move.json",
 *       "originRole": "CurrentPart",      // 省略可。この役の物の位置と向きを原点にする
 *       "blendIn": 0.5, "blendOut": 0.5, "lockInput": false, "skippable": false }
 *   ] }
 *
 * - ゲーム側は Fire("PartStarted") と名前で呼ぶだけ。何を再生するかは表が決める（ディレクタはシーケンサ）
 * - 役への割り当ては、先に「提供役」（SetProvider）、次に同じ名前・タグの GameObject の自動割り当てで行う
 * - 提供役は「今の患者」「今の部位」のように、ゲームだけが知っている物を役の名前で渡す窓口
 * - 表に無いイベントは何もしない
 */
class SequenceTriggers
{
public:
	/** @brief 表の1行 */
	struct Entry
	{
		std::string event;
		std::string sequence;
		std::string originRole;
		float blendIn = 0.5f;
		float blendOut = 0.5f;
		bool lockInput = false;
		bool skippable = false;
	};

	/** @brief JSON を読む（Resources/json からの相対）。読めなければ偽。表は空になる */
	bool Load(const std::string& relativePath);
	/** @brief 今の表を同じファイルに書く */
	bool Save() const;

	/**
	 * @brief 役に渡す物を返す関数を登録する
	 * @param role 役の名前（シーケンス側の役と同じ）
	 * @param provider 今その役にあたる GameObject を返す。いなければ nullptr
	 */
	void SetProvider(const std::string& role, std::function<GameObject*()> provider);
	void ClearProviders() { providers_.clear(); }

	/**
	 * @brief イベントに対応する行を全部再生する（後の行が前を中断する）
	 * @param eventNames 例 "PartStarted"
	 * @return 再生を始めた行があれば真
	 */
	bool Fire(const std::string& eventName) const;

	const std::vector<Entry>& GetEntries() const { return entries_; }

#ifdef USE_IMGUI
	/** @brief 表の確認と手動発火（デバッグ用） */
	void DrawImGui();
#endif

private:
	std::vector<Entry> entries_;
	std::unordered_map<std::string, std::function<GameObject*()>> providers_;
	std::string path_;
#ifdef USE_IMGUI
	std::string debugEvent_;
#endif
};
} // namespace KCE
