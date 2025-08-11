#pragma once
#include <chrono>

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
    static TimeManager& GetInstance();

    // グローバルタイムスケール
    void SetTimeScale(float scale);
    float GetTimeScale() const;

    // 一時停止
    void Pause();
    void Resume();
    bool IsPaused() const;

    // 経過時間取得
    float GetGameTime() const;
    float GetRealGameTime() const;

    // 毎フレーム呼び出す（引数不要）
    void Update();

    // deltaTime取得（タイムスケール適用済み/未適用）
    float GetDeltaTime() const;
    float GetRealDeltaTime() const;

private:
    // シングルトンインスタンス
    // ※外部から生成・コピー・ムーブできないようにする
    TimeManager();
    TimeManager(const TimeManager&) = delete;               // コピーコンストラクタ禁止
    TimeManager& operator=(const TimeManager&) = delete;    // コピー代入禁止
    TimeManager(TimeManager&&) = delete;                    // ムーブコンストラクタ禁止
    TimeManager& operator=(TimeManager&&) = delete;         // ムーブ代入禁止

    float timeScale_ = 1.0f;                                // タイムスケール
    bool paused_ = false;                                   // ゲーム一時停止フラグ
    float gameTime_ = 0.0f;                                 // タイムスケール・ポーズ適用済み累積経過時間（ゲーム用）
    float realGameTime_ = 0.0f;                         // タイムスケール・ポーズ未適用の累積経過時間（実時間用）
    float deltaTime_ = 0.0f;                                // タイムスケール・ポーズ適用済み前フレーム経過時間（ゲーム用）
    float realDeltaTime_ = 0.0f;                            // タイムスケール・ポーズ未適用前フレーム経過時間（UIやシステム用）

    std::chrono::steady_clock::time_point lastUpdate_;      // 前回Update呼び出し時刻
};