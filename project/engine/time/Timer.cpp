#include "Timer.h"

Timer::Timer(const std::string& name, float duration, DeltaTimeType deltaType)
	: name_(name), duration_(duration), elapsed_(0.0f), running_(false), finished_(false), deltaTimeType_(deltaType)
{
}

void Timer::Start()
{
    running_ = true;
    finished_ = false;
    elapsed_ = 0.0f;
	onStart(); // 開始時のコールバックを呼び出す
}

void Timer::Reset()
{
    running_ = false;
    finished_ = false;
    elapsed_ = 0.0f;
}

void Timer::Stop()
{
    running_ = false;
}

void Timer::Update(float deltaTime)
{
    if (!running_ || finished_) return;

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
    return duration_ - elapsed_;
}

std::string Timer::GetName() const
{
    return name_;
}