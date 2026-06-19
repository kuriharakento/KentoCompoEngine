#include "TimerManager.h"

#include "TimeManager.h"
#include "imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"

TimerManager& TimerManager::GetInstance()
{
	// 静的ローカル変数でシングルトンを実現
	static TimerManager instance;
	return instance;
}

TimerManager::TimerManager()
{
    // タイマーマップを初期化
    Clear();

#ifdef USE_IMGUI
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Timer Manager", [this]() { this->DrawImGui(); }, DebugUIArea::Inspector);
#endif
}

TimerManager::~TimerManager()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
    // 全タイマーを解放
    Clear();
}

void TimerManager::AddTimer(const std::string& name, float duration, DeltaTimeType deltaType)
{
    // 同名のタイマーが存在しない場合のみ追加
    if (timers_.find(name) == timers_.end())
    {
        // タイマーを生成して追加
        timers_[name] = std::make_unique<Timer>(name, duration, deltaType);
		// 追加と同時に開始
		timers_[name]->Start();
    }
}

void TimerManager::AddTimer(std::unique_ptr<Timer> timer)
{
    // 有効なタイマーで、同名が存在しない場合のみ追加
	if (timer && timers_.find(timer->GetName()) == timers_.end())
	{
		std::string name = timer->GetName();
		timers_[name] = std::move(timer);
		// 追加と同時に開始
		timers_[name]->Start();
	}
}

Timer* TimerManager::GetTimer(const std::string& name)
{
    // 名前でタイマーを検索
    auto it = timers_.find(name);
    if (it != timers_.end())
    {
        return it->second.get();
    }
    return nullptr;
}


void TimerManager::Update()
{
    // 全タイマーを更新
    for (auto it = timers_.begin(); it != timers_.end(); )
    {
        // 時間経過のタイプに応じて適切なデルタタイムで更新
        if (it->second->GetDeltaTimeType() == DeltaTimeType::DeltaTime)
        {
            // タイムスケール適用済みの時間で更新
            it->second->Update(TimeManager::GetInstance().GetGameContext().deltaTime);
        }
        else
        {
            // 実時間で更新
            it->second->Update(TimeManager::GetInstance().GetGameContext().realDeltaTime);
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
    // 全タイマーを削除
    timers_.clear();
}

void TimerManager::RemoveTimer(const std::string& name)
{
    // 名前で検索して削除
    auto it = timers_.find(name);
    if (it != timers_.end())
    {
        timers_.erase(it);
    }
}

bool TimerManager::HasTimer(const std::string& name) const
{
    // 指定名のタイマーが存在するかを確認
    return timers_.find(name) != timers_.end();
}

#ifdef USE_IMGUI
void TimerManager::DrawImGui()
{
	ImGui::SeparatorText("Active Timers");
	if (timers_.empty())
	{
		ImGui::TextDisabled("No active timers.");
		return;
	}

	for (const auto& timer : timers_)
	{
		const Timer* t = timer.second.get();
		ImGui::Text("Name:      %s", t->GetName().c_str());
		ImGui::Text("Remaining: %.2f s", t->GetRemainingTime());
		ImGui::Text("Running:   %s", t->IsRunning()  ? "true" : "false");
		ImGui::Text("Finished:  %s", t->IsFinished() ? "true" : "false");
		ImGui::Separator();
	}
}
#endif