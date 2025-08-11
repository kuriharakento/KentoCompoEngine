#include "TimerManager.h"

#include "TimeManager.h"
#include "imgui/imgui.h"

TimerManager& TimerManager::GetInstance()
{
	static TimerManager instance;
	return instance;
}

TimerManager::TimerManager()
{
    Clear();
}

TimerManager::~TimerManager()
{
    Clear();
}

void TimerManager::AddTimer(const std::string& name, float duration, DeltaTimeType deltaType)
{
    if (timers_.find(name) == timers_.end())
    {
        timers_[name] = std::make_unique<Timer>(name, duration, deltaType);
		timers_[name]->Start();
    }
}

void TimerManager::AddTimer(std::unique_ptr<Timer> timer)
{
	if (timer && timers_.find(timer->GetName()) == timers_.end())
	{
		std::string name = timer->GetName();
		timers_[name] = std::move(timer);
		timers_[name]->Start();
	}
}

Timer* TimerManager::GetTimer(const std::string& name)
{
    auto it = timers_.find(name);
    if (it != timers_.end())
    {
        return it->second.get();
    }
    return nullptr;
}

void TimerManager::Update()
{
#ifdef _DEBUG
    if (ImGui::Begin("TimerManager"))
    {
        ImGui::SeparatorText("List");
        for (const auto& timer : timers_)
        {
            const Timer* t = timer.second.get();
            ImGui::Text("Name: %s", t->GetName().c_str());
            ImGui::Text(" Remaining: %.2f", t->GetRemainingTime());
            ImGui::Text(" Running: %s", t->IsRunning() ? "true" : "false");
            ImGui::Text(" Finished: %s", t->IsFinished() ? "true" : "false");
            ImGui::Separator();
        }
    }
    ImGui::End();
#endif

    for (auto it = timers_.begin(); it != timers_.end(); )
    {
        // 時間経過のタイプに応じて更新
        if (it->second->GetDeltaTimeType() == DeltaTimeType::DeltaTime)
        {
            it->second->Update(TimeManager::GetInstance().GetDeltaTime());
        }
        else
        {
            it->second->Update(TimeManager::GetInstance().GetRealDeltaTime());
        }

        // タイマーが終了したら削除
        if (it->second->IsFinished())
        {
            it = timers_.erase(it); // eraseは次の有効なイテレータを返す
        }
        else
        {
            ++it;
        }
    }
}

void TimerManager::Clear()
{
    timers_.clear();
}

bool TimerManager::HasTimer(const std::string& name) const
{
    return timers_.find(name) != timers_.end();
}