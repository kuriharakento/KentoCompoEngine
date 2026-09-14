#pragma once
#include <string>
#include <vector>

#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief 文字の出し方
 * @details JSON には文字列で保存する。
 */
enum class TextTrackKind
{
	Lyric,	  //!< 歌詞テロップ（画面下の中央、フェードで出入り）
	Dialogue, //!< 会話（枠・話者名・タイプライター送り）
};

/**
 * @brief 1つの歌詞や台詞
 */
struct TextEntry
{
	float start = 0.0f;
	float duration = 3.0f;
	// 会話のときだけ使う
	std::string speaker;
	// UTF-8。改行は '\n'
	std::string text;
};

/**
 * @brief 歌詞テロップや会話を時刻で出し分けるトラック
 *
 * @details 役は使わない。出力先は BindingContext の TextOverlay。
 *          Evaluate(t) は「t に掛かっている行」だけで表示が決まるので、
 *          スクラブやスキップでもそのまま正しく出る。
 *          同じ種類のトラックが2本あると後に評価した方が勝つ。
 */
class TextTrack : public ITrack
{
public:
	TextTrack();

	TrackType GetType() const override { return TrackType::Text; }
	const char* GetTypeName() const override { return "Text"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override;

	/** @brief 表示は Evaluate だけで決まるので、退避するものは無い */
	void CaptureState(const BindingContext& ctx) override { (void)ctx; }
	/** @brief 自分の種類の表示を消す */
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return 1; }
	ICurveChannel* GetChannel(size_t index) override { return index == 0 ? &channel_ : nullptr; }

	/** @brief 指定時刻に新しい行を置く */
	bool RecordKey(float time, const BindingContext& ctx) override;

#ifdef USE_IMGUI
	bool DrawInspector() override;
#endif

	TextTrackKind GetKind() const { return kind_; }
	void SetKind(TextTrackKind kind) { kind_ = kind; }
	const std::vector<TextEntry>& GetEntries() const { return entries_; }

private:
	/**
	 * @brief t に掛かっている行を探す
	 * @return 重なっていたら後から始まった方。無ければ nullptr
	 */
	const TextEntry* FindActiveEntry(float time) const;

	/**
	 * @brief 行の並びを ICurveChannel として見せる窓口
	 * @details タイムラインでは行の開始時刻がキーとして並ぶ。補間はしない。
	 */
	class TextChannel : public ICurveChannel
	{
	public:
		TextChannel(std::vector<TextEntry>* entries, const TextTrackKind* kind) : entries_(entries), kind_(kind) {}

		const char* GetName() const override { return "Lines"; }
		size_t GetKeyCount() const override { return entries_->size(); }
		float GetKeyTime(size_t index) const override { return (*entries_)[index].start; }
		size_t MoveKey(size_t index, float newTime) override;
		void RemoveKey(size_t index) override;
		InterpolationMode& GetKeyInterp(size_t index) override { (void)index; return unusedInterp_; }
		BezierHandle& GetKeyBezier(size_t index) override { (void)index; return unusedBezier_; }
		bool HasInterpolation() const override { return false; }
		bool AddKeyAt(float time) override;

		/** @brief 開始時刻順を保ったまま追加する。追加後のインデックスを返す */
		size_t Insert(const TextEntry& entry);

#ifdef USE_IMGUI
		bool DrawKeyValueEditor(size_t index) override;
#endif

	private:
		std::vector<TextEntry>* entries_;
		// 会話のときだけ話者名の欄を出すために見る
		const TextTrackKind* kind_;
		InterpolationMode unusedInterp_ = InterpolationMode::Constant;
		BezierHandle unusedBezier_{};
	};

	TextTrackKind kind_ = TextTrackKind::Lyric;
	// 会話の文字送りの速さ（1秒あたりの文字数）
	float charsPerSecond_ = 20.0f;
	// 開始時刻の昇順に保たれた行
	std::vector<TextEntry> entries_;
	TextChannel channel_{ &entries_, &kind_ };
};
} // namespace KCE
