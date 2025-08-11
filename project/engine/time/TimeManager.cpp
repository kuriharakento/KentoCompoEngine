#include "TimeManager.h"

#include "imgui/imgui.h"

TimeManager& TimeManager::GetInstance()
{
    static TimeManager instance;
    return instance;
}

TimeManager::TimeManager()
{
    lastUpdate_ = std::chrono::steady_clock::now();
}

void TimeManager::Update()
{
#ifdef _DEBUG
    ImGui::Begin("Time Manager");

    // タイムスケールの操作
    ImGui::SliderFloat("Time Scale", &timeScale_, 0.0f, 3.0f, "%.2f");

    // ポーズのON/OFF
    ImGui::Checkbox("Paused", &paused_);

    // 経過時間表示
    ImGui::Text("GameTime: %.2f", gameTime_);
    ImGui::Text("UnscaledGameTime: %.2f", realGameTime_);
    ImGui::Text("DeltaTime: %.4f", deltaTime_);
    ImGui::Text("RealDeltaTime: %.4f", realDeltaTime_);

    ImGui::End();
#endif

    auto now = std::chrono::steady_clock::now();
    realDeltaTime_ = std::chrono::duration<float>(now - lastUpdate_).count();
    lastUpdate_ = now;

    // ポーズ時はdeltaTime_を0にする
    deltaTime_ = paused_ ? 0.0f : realDeltaTime_ * timeScale_;

    // 経過時間も加算
    if (!paused_)
    {
        gameTime_ += deltaTime_;
        realGameTime_ += realDeltaTime_;
    }
}

void TimeManager::SetTimeScale(float scale)
{
	timeScale_ = scale;
}

float TimeManager::GetTimeScale() const
{
	return timeScale_;
}

void TimeManager::Pause()
{
	paused_ = true;
}

void TimeManager::Resume()
{
	paused_ = false;
}

bool TimeManager::IsPaused() const
{
	return paused_;
}

float TimeManager::GetGameTime() const
{
	return gameTime_;
}

float TimeManager::GetRealGameTime() const
{
	return realGameTime_;
}

float TimeManager::GetDeltaTime() const
{
	return deltaTime_;
}

float TimeManager::GetRealDeltaTime() const
{
	return realDeltaTime_;
}