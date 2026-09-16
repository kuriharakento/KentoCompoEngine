#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "time/ClockId.h"
// 使う側が TimeManager.h だけで ClockRef も使えるように
#include "time/ClockRef.h"

namespace KCE
{
/**
 * @brief 1本の時計の、このフレームの時間
 * @details deltaTime は倍率を掛けた値、realDeltaTime は倍率を掛けない値。どちらも時計か親が止まっていれば 0
 */
struct TimeContext
{
	float deltaTime = 0.0f;        // 倍率を掛けた、1フレームの経過時間
	float gameTime = 0.0f;         // deltaTime の累計
	float realDeltaTime = 0.0f;    // 倍率を掛けない、1フレームの経過時間（止まっていれば 0）
	float realGameTime = 0.0f;     // realDeltaTime の累計
	float timeScale = 1.0f;        // この時計の倍率（1.0 が標準速度）
};

/**
 * @brief 時計（時間の流れ）を管理する
 *
 * - 時計は親子でつながり、deltaTime は「親の deltaTime × 自分の倍率」になる。自分か親が止まっていれば 0
 * - 最初からあるのは4本。エンジンの都合で要る分だけで、ゲームごとの時計（Player・Enemy など）はアプリが足す
 *   - Real  : 実時間の根っこ。止まらない・倍率 1
 *   - Game  : Real の子。ゲームの時間。一時停止・ヒットストップ・倍率が効く
 *   - UI    : Real の子。ゲームを止めても動く UI 用
 *   - Editor: Real の子。編集中に動かすもの（シーケンサの再生、カットシーン、モニターの描き直しなど）
 * - アプリは起動時に CreateClock で好きな時計を足せる。シーンではなく起動時に作ると、どのシーンやエディタからでも同じ名前で使える
 * - 時計は名前でも ClockId でも、名前を1回だけ探して覚える ClockRef でも指せる
 * - 時計そのものはここが持つ。使う側が覚えるのは名前か ClockId だけ
 * - 1フレームの経過時間は上限で抑える（読み込みやブレークポイントの後に物が飛ばないように）
 * - Update は毎フレーム1回、メインスレッドから呼ぶ
 */
class TimeManager
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return TimeManagerのインスタンス参照
	 */
	static TimeManager& GetInstance();

	/** @brief 経過時間を測って、全部の時計を進める。毎フレーム1回呼ぶ */
	void Update();

	/** @brief Settings のページ（システム > タイムマネージャー）を登録する。DebugUIManager の初期化の後に呼ぶ */
	void RegisterDebugUI();
	/** @brief Settings のページを外す。DebugUIManager の終了より前に呼ぶ */
	void UnregisterDebugUI();

	// --- 最初からある時計 ---

	/** @brief 実時間の根っこ。止まらない */
	ClockId RealClock() const;
	/** @brief ゲームの時計（Real の子）。一時停止・ヒットストップ・倍率が効く */
	ClockId GameClock() const;
	/** @brief UI の時計（Real の子）。ゲームの一時停止の影響を受けない */
	ClockId UIClock() const;
	/** @brief 編集中に動かすものの時計（Real の子）。シーケンサの再生・カットシーン・モニターの描き直しなど */
	ClockId EditorClock() const;

	// --- 時計を足す・探す・消す ---

	/**
	 * @brief 時計を足す
	 * @param name 名前。同じ名前の時計があればそれを返す
	 * @param parent 親の時計。無効なら Game
	 * @return 足した（か見つかった）時計。名前で使うなら受け取らなくていい
	 */
	ClockId CreateClock(const std::string& name, ClockId parent);

	/**
	 * @brief Game の子として時計を足す
	 * @param name 名前。同じ名前の時計があればそれを返す
	 * @return 足した（か見つかった）時計。名前で使うなら受け取らなくていい
	 */
	ClockId CreateClock(const std::string& name) { return CreateClock(name, GameClock()); }

	/**
	 * @brief 名前で時計を探す
	 * @return 見つかった時計。無ければ指定なしの ClockId
	 */
	ClockId FindClock(std::string_view name) const;

	/** @brief 時計を消す。子も一緒に消える。最初からある4本は消せない */
	void RemoveClock(ClockId clock);

	/** @brief 今もある時計か */
	bool IsValid(ClockId clock) const;

	// --- 時計の時間（ClockId で） ---

	/**
	 * @brief 時計のこのフレームの時間
	 * @param clock 時計。無効か指定なしなら Game として扱う
	 */
	const TimeContext& GetContext(ClockId clock) const;
	/** @brief 時計の、倍率を掛けた1フレームの経過時間 */
	float GetDeltaTime(ClockId clock) const { return GetContext(clock).deltaTime; }
	/** @brief 時計の、倍率を掛けない1フレームの経過時間（止まっていれば 0） */
	float GetRealDeltaTime(ClockId clock) const { return GetContext(clock).realDeltaTime; }

	// --- 時計の時間（名前で） ---

	/**
	 * @brief 名前の時計のこのフレームの時間
	 * @param name 時計の名前。無ければ Game の時間を返し、警告を名前ごとに1回だけ出す
	 */
	const TimeContext& GetContext(std::string_view name) const;
	/** @brief 名前の時計の、倍率を掛けた1フレームの経過時間 */
	float GetDeltaTime(std::string_view name) const { return GetContext(name).deltaTime; }
	/** @brief 名前の時計の、倍率を掛けない1フレームの経過時間（止まっていれば 0） */
	float GetRealDeltaTime(std::string_view name) const { return GetContext(name).realDeltaTime; }

	// --- 時計の操作（ClockId で。Real には効かない） ---

	/** @brief 時計の倍率を変える（1.0 が標準、0.5 で半速、0 で止まる） */
	void SetTimeScale(ClockId clock, float scale);
	/** @brief 時計を止める。子も止まる */
	void Pause(ClockId clock);
	/** @brief 時計を動かす */
	void Resume(ClockId clock);
	/** @brief 時計そのものが止められているか（親が止まっているかは見ない） */
	bool IsPaused(ClockId clock) const;
	/**
	 * @brief 指定した実時間だけ、その時計（と子）の更新時間を 0 にする
	 * @param durationSeconds ヒットストップ時間（秒）。負数は 0 として扱う
	 */
	void StartHitStop(ClockId clock, float durationSeconds);

	// --- 時計の操作（名前で。無い名前には何もせず、警告を名前ごとに1回だけ出す。間違えて Game を止めないように） ---

	/** @brief 名前の時計の倍率を変える */
	void SetTimeScale(std::string_view name, float scale);
	/** @brief 名前の時計を止める。子も止まる */
	void Pause(std::string_view name);
	/** @brief 名前の時計を動かす */
	void Resume(std::string_view name);
	/** @brief 名前の時計そのものが止められているか。無い名前は偽 */
	bool IsPaused(std::string_view name) const;
	/** @brief 名前の時計でヒットストップする */
	void StartHitStop(std::string_view name, float durationSeconds);

	/** @brief 起動してから何フレーム目か */
	uint64_t GetFrameCount() const { return frameCount_; }

	// --- 今までの口（Game と UI の時計に向ける） ---

	/** @brief ゲームを一時停止 */
	void Pause() { Pause(GameClock()); }
	/** @brief ゲームが一時停止中か */
	bool IsPaused() const { return IsPaused(GameClock()); }
	/** @brief ゲームの一時停止を解除 */
	void Resume() { Resume(GameClock()); }
	/** @brief ゲームの時計の倍率を変える */
	void SetGameTimeScale(float scale) { SetTimeScale(GameClock(), scale); }
	/** @brief UI の時計の倍率を変える */
	void SetUITimeScale(float scale) { SetTimeScale(UIClock(), scale); }
	/** @brief ゲームの時計でヒットストップする */
	void StartHitStop(float durationSeconds) { StartHitStop(GameClock(), durationSeconds); }
	/** @brief ゲームの時計の時間（ポーズの影響を受ける） */
	const TimeContext& GetGameContext() const { return GetContext(GameClock()); }
	/** @brief UI の時計の時間（ポーズの影響を受けない） */
	const TimeContext& GetUIContext() const { return GetContext(UIClock()); }

private:
	// 名前から番号を覚えるとき、見つからなければ警告を出すため
	friend class ClockRef;

	TimeManager();
	TimeManager(const TimeManager&) = delete;
	TimeManager& operator=(const TimeManager&) = delete;
	TimeManager(TimeManager&&) = delete;
	TimeManager& operator=(TimeManager&&) = delete;

	/** @brief 時計1本 */
	struct Clock
	{
		std::string name;
		// 親の番号。Real だけ ClockId::kInvalidIndex
		uint32_t parent = ClockId::kInvalidIndex;
		// 消すたびに増やす。古い ClockId を無効にするため
		uint32_t generation = 0;
		bool alive = true;
		bool paused = false;
		// ヒットストップの残り（実時間の秒）
		float hitStopRemaining = 0.0f;
		TimeContext context;
	};

	// 最初からある時計の番号
	static constexpr uint32_t kRealIndex = 0;
	static constexpr uint32_t kGameIndex = 1;
	static constexpr uint32_t kUIIndex = 2;
	static constexpr uint32_t kEditorIndex = 3;
	// ここまでは消せない
	static constexpr uint32_t kBuiltInClockCount = 4;

	/** @brief ClockId を配列の番号にする。無効か指定なしなら Game */
	uint32_t Resolve(ClockId clock) const;
	/** @brief 名前から配列の番号を探す。無ければ ClockId::kInvalidIndex */
	uint32_t FindIndex(std::string_view name) const;
	/** @brief 名前で探して、無ければ警告を出す（名前ごとに1回だけ） @return 無ければ ClockId::kInvalidIndex */
	uint32_t FindIndexOrWarn(std::string_view name) const;
	/** @brief 配列の番号から ClockId を作る */
	ClockId MakeId(uint32_t index) const;
	/** @brief 最初からある時計を1本作る */
	void AddBuiltInClock(const std::string& name, uint32_t parent);
#ifdef USE_IMGUI
	/** @brief 時計を親子の木で並べる */
	void DrawImGui();
	/** @brief 時計1本とその子を描く */
	void DrawClockTree(uint32_t index);
#endif

	// 作った順に並ぶ。親は必ず子より前にある（作るときに親が要るので）。
	// 消した時計は alive=false で残し、番号は使い回さない（並びの決まりを崩さないため）
	std::vector<std::unique_ptr<Clock>> clocks_;
	// 見つからないと警告した名前。同じ名前で毎フレーム警告しないように覚える（増えるのは打ち間違えたときだけ）
	mutable std::vector<std::string> warnedNames_;
	uint64_t frameCount_ = 0;
	// 前回 Update した時刻
	std::chrono::steady_clock::time_point lastUpdate_;
};
} // namespace KCE
