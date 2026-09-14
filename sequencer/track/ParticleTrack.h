#pragma once
#include <string>
#include <vector>

#include "math/Vector3.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief シーケンス上の1回ぶんのパーティクル
 */
struct ParticleCue
{
	float time = 0.0f;
	// ParticleManager に登録されたエフェクト名
	std::string effect;
	// 出す位置。役が空のときに使う（シーケンスの原点からの位置）
	Vector3 position = { 0.0f, 0.0f, 0.0f };
	// 空でなければ、この役の GameObject の位置に出す（出した瞬間の位置。後は追わない）
	std::string role;
	// スキップしたときにも出すか。見た目だけの演出なので既定は偽
	bool fireOnSkip = false;
};

/**
 * @brief 指定した時刻にパーティクルのエフェクトを出すトラック
 *
 * @details パーティクルは前のフレームから積み上げて動くので、Evaluate(t) の純関数契約に乗らない
 *          （ITrack.h の例外）。ここでは EventTrack と同じく「その時刻を通過したとき」に出すだけにする。
 *          - 再生中に通過したら出す。発火は SequencePlayer が前フレームの時刻から今の時刻までの範囲で行う
 *          - スクラブ（Seek）や途中から再生では出さない。巻き戻しても、出たものは消さない（寿命で消える）
 *          - 追従（Play(name, Transform*)）は使わない。対象が先に破棄されると Transform を指したままになるため
 */
class ParticleTrack : public ITrack
{
public:
	ParticleTrack();

	TrackType GetType() const override { return TrackType::Particle; }
	const char* GetTypeName() const override { return "Particle"; }

	/** @brief 状態を持たないので何もしない（発火は SequencePlayer が行う） */
	void Evaluate(float time, const BindingContext& ctx) override { (void)time; (void)ctx; }
	float GetEndTime() const override;

	void CaptureState(const BindingContext& ctx) override { (void)ctx; }
	void RestoreState(const BindingContext& ctx) override { (void)ctx; }

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return 1; }
	ICurveChannel* GetChannel(size_t index) override { return index == 0 ? &channel_ : nullptr; }

	/** @brief 指定時刻に新しいキューを置く */
	bool RecordKey(float time, const BindingContext& ctx) override;

	/**
	 * @brief (from, to] の範囲にあるキューのエフェクトを出す
	 * @param from 範囲の始まり（この時刻ちょうどは含まない）
	 * @param to 範囲の終わり（この時刻ちょうどは含む）
	 * @param skipping スキップ中なら真。fireOnSkip が偽のキューは出さない
	 * @param ctx 役の GameObject と原点を引くコンテキスト
	 */
	void FireInRange(float from, float to, bool skipping, const BindingContext& ctx) const;

	const std::vector<ParticleCue>& GetCues() const { return cues_; }

private:
	/**
	 * @brief キュー列を ICurveChannel として見せる窓口
	 * @details 補間しないので、補間とベジェは使われない値を返す（EventTrack と同じ）。
	 */
	class CueChannel : public ICurveChannel
	{
	public:
		explicit CueChannel(std::vector<ParticleCue>* cues) : cues_(cues) {}

		const char* GetName() const override { return "Particles"; }
		size_t GetKeyCount() const override { return cues_->size(); }
		float GetKeyTime(size_t index) const override { return (*cues_)[index].time; }
		size_t MoveKey(size_t index, float newTime) override;
		void RemoveKey(size_t index) override;
		InterpolationMode& GetKeyInterp(size_t index) override { (void)index; return unusedInterp_; }
		BezierHandle& GetKeyBezier(size_t index) override { (void)index; return unusedBezier_; }
		bool HasInterpolation() const override { return false; }
		nlohmann::json CopyKey(size_t index) const override;
		/** @details 同じ時刻に複数あってよいので、上書きせずに足す */
		bool PasteKey(float time, const nlohmann::json& json) override;
		bool AddKeyAt(float time) override;

		/** @brief 時刻順を保ったまま追加する。追加後のインデックスを返す */
		size_t Insert(const ParticleCue& cue);

#ifdef USE_IMGUI
		bool DrawKeyValueEditor(size_t index) override;
#endif

	private:
		std::vector<ParticleCue>* cues_;
		InterpolationMode unusedInterp_ = InterpolationMode::Constant;
		BezierHandle unusedBezier_{};
	};

	// 時刻の昇順に保たれたキュー
	std::vector<ParticleCue> cues_;
	CueChannel channel_{ &cues_ };
};
} // namespace KCE
