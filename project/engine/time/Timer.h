#pragma once
#include <string>
#include <functional>

enum class DeltaTimeType
{
	DeltaTime,      // タイムスケール適用済み
	RealDeltaTime, // タイムスケール未適用
};

class Timer
{
public:
    // コンストラクタ
    Timer(const std::string& name, float duration, DeltaTimeType deltaType = DeltaTimeType::DeltaTime);

    // タイマー操作
    void Start();
    void Reset();
    void Stop();

    // 毎フレーム呼び出し
    void Update(float deltaTime);

    // コールバック設定
    void SetOnStart(std::function<void()> callback); // 開始時
    void SetOnTick(std::function<void(float)> callback);      // 毎フレーム（残り時間通知など）
    void SetOnFinish(std::function<void()> callback);         // 終了時

    // 状態取得
    bool IsRunning() const;
    bool IsFinished() const;
    float GetRemainingTime() const;
    std::string GetName() const;
	DeltaTimeType GetDeltaTimeType() const { return deltaTimeType_; }

private:
    std::string name_;
    float duration_;
    float elapsed_;
    bool running_;
    bool finished_;

	// 時間経過のタイプ
	DeltaTimeType deltaTimeType_ = DeltaTimeType::DeltaTime;

    std::function<void()> onStart;
    std::function<void(float)> onTick_;
    std::function<void()> onFinish_;
};