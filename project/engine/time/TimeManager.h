#pragma once
#include <chrono>

/**
 * @brief 時間コンテキスト構造体
 * 
 * ゲーム内の時間情報を格納する。タイムスケールの適用有無で
 * 2種類の時間情報（ゲーム時間と実時間）を管理する。
 */
struct TimeContext
{
    float deltaTime;        // スケール適用済みの１フレーム経過時間
    float gameTime;         // スケール適用済みの累積経過時間
    float realDeltaTime;    // 実時間での１フレーム経過時間
    float realGameTime;     // 実時間での累積経過時間
    float timeScale;        // このコンテキストのタイムスケール（1.0が標準速度）
};

/**
 * @brief ゲーム全体の時間管理を行うクラス
 *
 * タイムスケール（スローや加速）、一時停止状態、経過時間、deltaTime等を管理します。
 * Update()を毎フレーム呼び出すことで、内部で時間計測を行い、
 * ゲームオブジェクトやシステムに時間情報を提供します。
 * 
 * ゲーム用とUI用の2つの時間コンテキストを持ち、
 * ポーズ中でもUIは更新を続けることができます。
 */
class TimeManager
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return TimeManagerのインスタンス参照
	 */
    static TimeManager& GetInstance();

    // --- 時間操作 ---

    /**
     * @brief ゲームを一時停止
     */
    void Pause();

    /**
     * @brief 一時停止状態かどうかを取得
     * @return 一時停止中ならtrue
     */
    bool IsPaused() const;

    /**
     * @brief ゲームの一時停止を解除
     */
	void Resume();

    /**
     * @brief 毎フレーム呼び出す更新関数
     * 
     * 経過時間を計測し、各コンテキストの時間情報を更新する。
     */
    void Update();

	// --- タイムスケール設定 ---

	/**
	 * @brief ゲーム用タイムスケールを設定
	 * @param scale タイムスケール（1.0が標準、0.5で半速、2.0で倍速）
	 */
	void SetGameTimeScale(float scale) { gameContext_.timeScale = scale; }

	/**
	 * @brief UI用タイムスケールを設定
	 * @param scale タイムスケール
	 */
	void SetUITimeScale(float scale) { uiContext_.timeScale = scale; }

    // --- 時間取得 ---

    /**
     * @brief ゲーム用時間コンテキストを取得
     * @return ゲーム用TimeContext（ポーズの影響を受ける）
     */
    const TimeContext& GetGameContext() const { return gameContext_; }

    /**
     * @brief UI用時間コンテキストを取得
     * @return UI用TimeContext（ポーズの影響を受けない）
     */
    const TimeContext& GetUIContext() const { return uiContext_; }


private:
    // シングルトンパターンのためコンストラクタをprivateに
    TimeManager();
    TimeManager(const TimeManager&) = delete;               // コピーコンストラクタ禁止
    TimeManager& operator=(const TimeManager&) = delete;    // コピー代入禁止
    TimeManager(TimeManager&&) = delete;                    // ムーブコンストラクタ禁止
    TimeManager& operator=(TimeManager&&) = delete;         // ムーブ代入禁止

private:
    /**
     * @brief 時間コンテキストを更新
     * @param context 更新するコンテキスト
     * @param realDelta 実時間での経過時間
     * @param isPaused ポーズ状態
     */
    void UpdateTimeContext(TimeContext& context, float realDelta, bool isPaused);

private:
	bool paused_ = false;                                   // ゲーム一時停止フラグ
    TimeContext gameContext_; // ゲーム用コンテキスト
    TimeContext uiContext_;   // UI用コンテキスト

    std::chrono::steady_clock::time_point lastUpdate_;      // 前回Update呼び出し時刻
};