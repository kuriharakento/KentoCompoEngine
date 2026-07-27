#include "Timer.h"

namespace KCE
{
void Timer::Start()
{
    // タイマーを開始状態に設定
    running_ = true;
    finished_ = false;
    elapsed_ = 0.0f;
	// 開始時のコールバックを呼び出す
	onStart();
}

void Timer::Reset()
{
    // タイマーをリセット
    running_ = false;
    finished_ = false;
    elapsed_ = 0.0f;
}

void Timer::Stop()
{
    // タイマーを停止（経過時間は保持）
    running_ = false;
}

void Timer::Update(float deltaTime)
{
    // 動作中でなければ何もしない
    if (!running_ || finished_) return;

    // 経過時間を加算
    elapsed_ += deltaTime;
    float remaining = duration_ - elapsed_;

    // 毎フレームコールバック
    if (onTick_)
    {
        onTick_(remaining > 0.0f ? remaining : 0.0f);
    }

    // 終了判定
    if (elapsed_ >= duration_)
    {
        running_ = false;
        finished_ = true;
        // 終了時コールバック
        if (onFinish_)
        {
            onFinish_();
        }
    }
}

void Timer::SetOnStart(std::function<void()> callback)
{
	onStart = callback;
}

void Timer::SetOnTick(std::function<void(float)> callback)
{
    onTick_ = callback;
}

void Timer::SetOnFinish(std::function<void()> callback)
{
    onFinish_ = callback;
}

bool Timer::IsRunning() const
{
    return running_;
}

bool Timer::IsFinished() const
{
    return finished_;
}

float Timer::GetRemainingTime() const
{
    // 残り時間を計算
    return duration_ - elapsed_;
}

float Timer::GetDuration() const
{
	return duration_;
}

float Timer::GetProgress() const
{
	return elapsed_ / duration_;
}

std::string Timer::GetName() const
{
    return name_;
}
} // namespace KCE
