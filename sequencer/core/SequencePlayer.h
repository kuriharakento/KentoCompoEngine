#pragma once
#include <functional>
#include <string>

#include "sequencer/core/BindingContext.h"
#include "sequencer/core/Sequence.h"

namespace KCE
{
/**
 * @brief シーケンスの再生状態
 */
enum class PlaybackState
{
	Stopped, //!< 停止中。対象は退避した状態に戻っている
	Playing, //!< 再生中
	Paused,	 //!< 一時停止中。時刻は保持される
};

/**
 * @brief シーケンスの再生位置を管理し、毎フレーム Evaluate を呼ぶクラス
 *
 * @section authority 時間の権威
 *
 * SEQUENCER_PLAN 3.8。シーケンスに音声が紐付いている場合、時刻は
 * **オーディオの再生位置を権威とする**。deltaTime の積算では
 * 数分の楽曲で必ずズレるため。
 *
 * 音声が無いシーケンス（カットシーンなど）は実時間で進める。
 * このとき使うのは realDeltaTime であり、ゲームのタイムスケールの
 * 影響は受けない。スロー演出（TimeScaleTrack）の最中もシーケンサ自身は
 * 等速で進む必要があるため（SEQUENCER_PLAN 7.6）。
 *
 * @section purity 純関数契約との関係
 *
 * このクラスだけが「時刻を進める」責務を持つ。トラック側は与えられた
 * 時刻を評価するだけなので、Seek() や SetTime() でどこへ飛んでも
 * 正しい絵になる。スキップは SkipToEnd() の一行で済む。
 */
class SequencePlayer
{
public:
	/**
	 * @brief 再生対象のシーケンスを設定する
	 * @details 再生中の場合は先に停止する。nullptr を渡すと対象を外す。
	 * @param sequence 対象のシーケンス。所有権は移らない
	 */
	void SetSequence(Sequence* sequence);

	Sequence* GetSequence() const { return sequence_; }

	/**
	 * @brief バインディングコンテキストへの参照を得る
	 * @details 再生前にここへ役と実体の対応を入れる。
	 * @return コンテキスト
	 */
	BindingContext& GetBindingContext() { return bindingContext_; }
	const BindingContext& GetBindingContext() const { return bindingContext_; }

	/**
	 * @brief 先頭から再生する
	 * @details 対象の状態を退避してから開始する。音声が設定されていれば
	 *          その再生も開始し、以降の時刻は音声の再生位置に従う。
	 */
	void Play();

	/**
	 * @brief 現在位置から再生を再開する
	 */
	void Resume();

	/**
	 * @brief 一時停止する
	 * @details 時刻は保持される。対象の状態は現在の見た目のまま残る。
	 */
	void Pause();

	/**
	 * @brief 停止し、対象を再生前の状態に戻す
	 * @details SEQUENCER_PLAN 3.5 の状態復元を行う。
	 */
	void Stop();

	/**
	 * @brief 指定時刻へ移動する
	 * @details 再生中・停止中を問わず使える。エディタのスクラブもこれを使う。
	 *          純関数契約が守られていれば、どこへ飛んでも正しい絵になる。
	 * @param time 移動先の時刻（秒）。0〜長さの範囲にクランプされる
	 */
	void Seek(float time);

	/**
	 * @brief 末尾へ飛ばす
	 *
	 * @details カットシーンのスキップの実体。純関数契約が守られていれば、
	 *          t = end を一発適用するだけで最終状態が得られる。
	 *          副作用の積み上げで実装されているとこれは成立しない。
	 */
	void SkipToEnd();

	/**
	 * @brief 毎フレーム呼ぶ更新処理
	 *
	 * @details 時刻を進めて Evaluate する。時刻の決め方は上記の通り、
	 *          音声があれば音声の再生位置、無ければ実時間。
	 *          停止中・一時停止中は時刻を進めない。
	 */
	void Update();

	/**
	 * @brief 現在時刻でシーケンスを評価し直す
	 * @details 編集モードでカーブを触った直後など、時刻を進めずに
	 *          絵だけ更新したいときに使う。
	 */
	void EvaluateCurrentTime();

	float GetTime() const { return time_; }
	PlaybackState GetState() const { return state_; }
	bool IsPlaying() const { return state_ == PlaybackState::Playing; }

	/**
	 * @brief ループ再生するかどうかを設定する
	 * @param loop ループするなら真
	 */
	void SetLoop(bool loop) { loop_ = loop; }
	bool IsLoop() const { return loop_; }

	/**
	 * @brief 音声を時間の権威として使うかどうか
	 * @details シーケンスに audioClip が設定されており、かつ有効な場合に真。
	 * @return 音声同期中なら真
	 */
	bool IsAudioDriven() const;

	/**
	 * @brief 対象シーケンスの長さ
	 * @return 長さ（秒）
	 */
	float GetDuration() const;

	/** @brief イベントを受け取るコールバック。引数はイベント名 */
	using EventCallback = std::function<void(const std::string& eventName)>;

	/**
	 * @brief EventTrack のイベントを受け取るコールバックを設定する
	 * @details 再生中に時刻を通過したときと、スキップ時（fireOnSkip のもの）に呼ばれる。
	 *          スクラブ（Seek）では呼ばれない。
	 */
	void SetEventCallback(EventCallback callback) { eventCallback_ = std::move(callback); }

private:
	/**
	 * @brief (from, to] の範囲にあるイベントを発火する
	 * @param from 範囲の始まり（含まない）
	 * @param to 範囲の終わり（含む）
	 * @param skipping スキップによる発火なら真
	 */
	void FireEvents(float from, float to, bool skipping);
	/**
	 * @brief 時刻を適用し、シーケンスを評価する
	 * @param time 適用する時刻（秒）
	 */
	void ApplyTime(float time);

	/**
	 * @brief 音声の再生を開始する（音声が設定されていれば）
	 * @param startTime 開始位置（秒）
	 */
	void StartAudio(float startTime);

	/**
	 * @brief 音声の再生を止める
	 */
	void StopAudio();

	// 再生対象。所有権は持たない
	Sequence* sequence_ = nullptr;
	// 役と実体の対応
	BindingContext bindingContext_;
	// 現在時刻（秒）
	float time_ = 0.0f;
	// 再生状態
	PlaybackState state_ = PlaybackState::Stopped;
	// ループ再生フラグ
	bool loop_ = false;
	// 状態を退避済みかどうか
	bool hasCapturedState_ = false;
	// 音声を再生中かどうか
	bool audioStarted_ = false;
	// イベントの受け取り先
	EventCallback eventCallback_;
};
} // namespace KCE
