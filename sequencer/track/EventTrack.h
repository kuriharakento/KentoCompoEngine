#pragma once
#include <functional>
#include <string>
#include <vector>

#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief シーケンス上の1つのイベント
 */
struct SequenceEvent
{
	float time = 0.0f;
	// ゲーム側がこの名前を見て処理を分ける（例: "PlayVoice_01", "SpawnEnemy"）
	std::string name = "Event";
	// スキップしたときにも呼ぶか。
	// 「敵を出す」のように結果が後の進行に要るものは真、
	// 「効果音を鳴らす」のように演出だけのものは偽にする
	bool fireOnSkip = true;
};

/**
 * @brief 指定した時刻にゲーム側のコールバックを呼ぶトラック
 *
 * @details SEQUENCER_PLAN 7.7「条件分岐・イベント発火」。
 *          イベントは「その時刻を通過したとき」に起きる副作用であり、
 *          純関数契約（Evaluate(t)）の対象外。発火は SequencePlayer が
 *          前フレームの時刻から今の時刻までの範囲で行う。
 *          スクラブ（エディタで時刻を掴んで動かす）では発火しない。
 */
class EventTrack : public ITrack
{
public:
	EventTrack();

	TrackType GetType() const override { return TrackType::Event; }
	const char* GetTypeName() const override { return "Event"; }

	/** @brief イベントは状態を持たないので何もしない（発火は SequencePlayer が行う） */
	void Evaluate(float time, const BindingContext& ctx) override { (void)time; (void)ctx; }
	float GetEndTime() const override;

	void CaptureState(const BindingContext& ctx) override { (void)ctx; }
	void RestoreState(const BindingContext& ctx) override { (void)ctx; }

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return 1; }
	ICurveChannel* GetChannel(size_t index) override { return index == 0 ? &channel_ : nullptr; }

	/** @brief 指定時刻に新しいイベントを置く */
	bool RecordKey(float time, const BindingContext& ctx) override;

	/**
	 * @brief (from, to] の範囲にあるイベントを時刻順に列挙する
	 * @param from 範囲の始まり（この時刻ちょうどは含まない）
	 * @param to 範囲の終わり（この時刻ちょうどは含む）
	 * @param skipping スキップ中なら真。fireOnSkip が偽のイベントは除く
	 * @param callback イベントごとに呼ばれる
	 */
	void ForEachEventInRange(float from, float to, bool skipping, const std::function<void(const SequenceEvent&)>& callback) const;

	const std::vector<SequenceEvent>& GetEvents() const { return events_; }

private:
	/**
	 * @brief イベント列を ICurveChannel として見せる窓口
	 * @details イベントは補間しないので、補間とベジェは使われない値を返す。
	 */
	class EventChannel : public ICurveChannel
	{
	public:
		explicit EventChannel(std::vector<SequenceEvent>* events) : events_(events) {}

		const char* GetName() const override { return "Events"; }
		size_t GetKeyCount() const override { return events_->size(); }
		float GetKeyTime(size_t index) const override { return (*events_)[index].time; }
		size_t MoveKey(size_t index, float newTime) override;
		void RemoveKey(size_t index) override;
		InterpolationMode& GetKeyInterp(size_t index) override { (void)index; return unusedInterp_; }
		BezierHandle& GetKeyBezier(size_t index) override { (void)index; return unusedBezier_; }
		bool HasInterpolation() const override { return false; }
		nlohmann::json CopyKey(size_t index) const override;
		/** @details イベントは同じ時刻に複数あってよいので、上書きせずに足す */
		bool PasteKey(float time, const nlohmann::json& json) override;
		bool AddKeyAt(float time) override;

		/** @brief 時刻順を保ったまま追加する。追加後のインデックスを返す */
		size_t Insert(const SequenceEvent& event);

#ifdef USE_IMGUI
		bool DrawKeyValueEditor(size_t index) override;
#endif

	private:
		std::vector<SequenceEvent>* events_;
		InterpolationMode unusedInterp_ = InterpolationMode::Constant;
		BezierHandle unusedBezier_{};
	};

	// 時刻の昇順に保たれたイベント
	std::vector<SequenceEvent> events_;
	EventChannel channel_{ &events_ };
};
} // namespace KCE
