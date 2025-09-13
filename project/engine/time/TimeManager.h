#pragma once
#include <chrono>

struct TimeContext
{
    float deltaTime;        // スケール適用済みの１フレーム経過時間
    float gameTime;         // スケール適用済みの累積経過時間
    float realDeltaTime;    // 実時間での１フレーム経過時間
    float realGameTime;     // 実時間での累積経過時間
    float timeScale;        // このコンテキストのタイムスケール
};

/**
 * @brief ゲーム全体の時間管理を行うクラス
 *
 * タイムスケール（スローや加速）、一時停止状態、経過時間、deltaTime等を管理します。
 * Update()を毎フレーム呼び出すことで、内部で時間計測を行い、
 * ゲームオブジェクトやシステムに時間情報を提供します。
 */
class TimeManager
{
public:
	// シングルトンインスタンス取得
    static TimeManager& GetInstance();

    // 時間操作
    void Pause();
    bool IsPaused() const;
	void Resume();

    // 毎フレーム呼び出す
    void Update();

	// タイムスケール設定
	void SetGameTimeScale(float scale) { gameContext_.timeScale = scale; }
	void SetUITimeScale(float scale) { uiContext_.timeScale = scale; }

    // 時間取得
    const TimeContext& GetGameContext() const { return gameContext_; }
    const TimeContext& GetUIContext() const { return uiContext_; }


private: // シングルトンインスタンス
    TimeManager();
    TimeManager(const TimeManager&) = delete;               // コピーコンストラクタ禁止
    TimeManager& operator=(const TimeManager&) = delete;    // コピー代入禁止
    TimeManager(TimeManager&&) = delete;                    // ムーブコンストラクタ禁止
    TimeManager& operator=(TimeManager&&) = delete;         // ムーブ代入禁止

private:
    void UpdateTimeContext(TimeContext& context, float realDelta, bool isPaused);

private:
	bool paused_ = false;                                   // ゲーム一時停止フラグ
    TimeContext gameContext_; // ゲーム用
    TimeContext uiContext_;   // UI用

    std::chrono::steady_clock::time_point lastUpdate_;      // 前回Update呼び出し時刻
};