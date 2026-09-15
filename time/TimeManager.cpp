#include "TimeManager.h"

#include <algorithm>

#include "base/Logger.h"
#include "time/ClockRef.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
// 1フレームの経過時間の上限（秒）。読み込みやブレークポイントで止まった後に、数秒分が一気に進んで物が飛ばないように
constexpr float kMaxDeltaTime = 0.1f;
// Settings の倍率のスライダーの上限
constexpr float kMaxTimeScaleSlider = 3.0f;
} // namespace

TimeManager& TimeManager::GetInstance()
{
	// 静的ローカル変数でシングルトンを実現
	static TimeManager instance;
	return instance;
}

TimeManager::TimeManager()
{
	lastUpdate_ = std::chrono::steady_clock::now();

	// 最初からある時計。番号を kRealIndex・kGameIndex・kUIIndex に揃える
	auto real = std::make_unique<Clock>();
	real->name = "Real";
	clocks_.push_back(std::move(real));

	auto game = std::make_unique<Clock>();
	game->name = "Game";
	game->parent = kRealIndex;
	clocks_.push_back(std::move(game));

	auto ui = std::make_unique<Clock>();
	ui->name = "UI";
	ui->parent = kRealIndex;
	clocks_.push_back(std::move(ui));
}

void TimeManager::Update()
{
	// 実時間の計測
	const auto now = std::chrono::steady_clock::now();
	const float measured = std::chrono::duration<float>(now - lastUpdate_).count();
	lastUpdate_ = now;
	const float realDelta = (std::min)(measured, kMaxDeltaTime);
	++frameCount_;

	Clock& real = *clocks_[kRealIndex];
	real.context.deltaTime = realDelta;
	real.context.realDeltaTime = realDelta;
	real.context.gameTime += realDelta;
	real.context.realGameTime += realDelta;

	// 親は子より前にあるので、前から1回回すだけで親子の計算が済む
	for (size_t i = kRealIndex + 1; i < clocks_.size(); ++i)
	{
		Clock& clock = *clocks_[i];
		if (!clock.alive)
		{
			continue;
		}
		const TimeContext& parent = clocks_[clock.parent]->context;

		// ヒットストップは実時間で減らす（その時計や親が止まっていても減る）
		const bool hitStopped = clock.hitStopRemaining > 0.0f;
		clock.hitStopRemaining = (std::max)(0.0f, clock.hitStopRemaining - realDelta);
		const bool stopped = clock.paused || hitStopped;

		clock.context.realDeltaTime = stopped ? 0.0f : parent.realDeltaTime;
		clock.context.deltaTime = stopped ? 0.0f : parent.deltaTime * clock.context.timeScale;
		clock.context.gameTime += clock.context.deltaTime;
		clock.context.realGameTime += clock.context.realDeltaTime;
	}
}

ClockId TimeManager::MakeId(uint32_t index) const
{
	return { index, clocks_[index]->generation };
}

ClockId TimeManager::RealClock() const { return MakeId(kRealIndex); }
ClockId TimeManager::GameClock() const { return MakeId(kGameIndex); }
ClockId TimeManager::UIClock() const { return MakeId(kUIIndex); }

bool TimeManager::IsValid(ClockId clock) const
{
	return clock.index < clocks_.size() && clocks_[clock.index]->alive && clocks_[clock.index]->generation == clock.generation;
}

uint32_t TimeManager::Resolve(ClockId clock) const
{
	return IsValid(clock) ? clock.index : kGameIndex;
}

uint32_t TimeManager::FindIndex(std::string_view name) const
{
	for (uint32_t i = 0; i < clocks_.size(); ++i)
	{
		if (clocks_[i]->alive && clocks_[i]->name == name)
		{
			return i;
		}
	}
	return ClockId::kInvalidIndex;
}

uint32_t TimeManager::FindIndexOrWarn(std::string_view name) const
{
	const uint32_t index = FindIndex(name);
	if (index != ClockId::kInvalidIndex)
	{
		return index;
	}
	// 毎フレーム同じ名前で呼ばれてもログが埋まらないよう、名前ごとに1回だけ出す
	if (std::find(warnedNames_.begin(), warnedNames_.end(), name) == warnedNames_.end())
	{
		warnedNames_.emplace_back(name);
		Logger::Log("TimeManager: 時計が見つかりません: " + std::string(name) + "（CreateClock で起動時に作っておく）\n", Logger::LogLevel::Warning);
	}
	return ClockId::kInvalidIndex;
}

ClockId TimeManager::CreateClock(const std::string& name, ClockId parent)
{
	const ClockId existing = FindClock(name);
	if (existing.IsSpecified())
	{
		return existing;
	}
	auto clock = std::make_unique<Clock>();
	clock->name = name;
	clock->parent = IsValid(parent) ? parent.index : kGameIndex;
	clocks_.push_back(std::move(clock));
	return MakeId(static_cast<uint32_t>(clocks_.size() - 1));
}

ClockId TimeManager::FindClock(std::string_view name) const
{
	const uint32_t index = FindIndex(name);
	return index != ClockId::kInvalidIndex ? MakeId(index) : ClockId{};
}

void TimeManager::RemoveClock(ClockId clock)
{
	if (!IsValid(clock) || clock.index <= kUIIndex)
	{
		return;
	}
	Clock& target = *clocks_[clock.index];
	target.alive = false;
	++target.generation;
	// 子は親より後ろにあるので、前から順に「親が消えた時計」を消していけば孫まで1回で消える
	for (size_t i = clock.index + 1; i < clocks_.size(); ++i)
	{
		Clock& child = *clocks_[i];
		if (child.alive && !clocks_[child.parent]->alive)
		{
			child.alive = false;
			++child.generation;
		}
	}
}

const TimeContext& TimeManager::GetContext(ClockId clock) const
{
	return clocks_[Resolve(clock)]->context;
}

const TimeContext& TimeManager::GetContext(std::string_view name) const
{
	const uint32_t index = FindIndexOrWarn(name);
	return clocks_[index != ClockId::kInvalidIndex ? index : kGameIndex]->context;
}

// 操作の口は、無い時計（消えた・見つかっていない ClockId）には何もしない。
// 読む口のように Game へ落とすと、名前の打ち間違いで Game を止めてしまうため
void TimeManager::SetTimeScale(ClockId clock, float scale)
{
	if (IsValid(clock) && clock.index != kRealIndex)
	{
		clocks_[clock.index]->context.timeScale = scale;
	}
}

void TimeManager::Pause(ClockId clock)
{
	if (IsValid(clock) && clock.index != kRealIndex)
	{
		clocks_[clock.index]->paused = true;
	}
}

void TimeManager::Resume(ClockId clock)
{
	if (IsValid(clock))
	{
		clocks_[clock.index]->paused = false;
	}
}

bool TimeManager::IsPaused(ClockId clock) const
{
	return IsValid(clock) && clocks_[clock.index]->paused;
}

void TimeManager::StartHitStop(ClockId clock, float durationSeconds)
{
	if (!IsValid(clock) || clock.index == kRealIndex)
	{
		return;
	}
	Clock& target = *clocks_[clock.index];
	target.hitStopRemaining = (std::max)(target.hitStopRemaining, (std::max)(0.0f, durationSeconds));
}

void TimeManager::SetTimeScale(std::string_view name, float scale)
{
	const uint32_t index = FindIndexOrWarn(name);
	if (index != ClockId::kInvalidIndex)
	{
		SetTimeScale(MakeId(index), scale);
	}
}

void TimeManager::Pause(std::string_view name)
{
	const uint32_t index = FindIndexOrWarn(name);
	if (index != ClockId::kInvalidIndex)
	{
		Pause(MakeId(index));
	}
}

void TimeManager::Resume(std::string_view name)
{
	const uint32_t index = FindIndexOrWarn(name);
	if (index != ClockId::kInvalidIndex)
	{
		Resume(MakeId(index));
	}
}

bool TimeManager::IsPaused(std::string_view name) const
{
	const uint32_t index = FindIndexOrWarn(name);
	return index != ClockId::kInvalidIndex && clocks_[index]->paused;
}

void TimeManager::StartHitStop(std::string_view name, float durationSeconds)
{
	const uint32_t index = FindIndexOrWarn(name);
	if (index != ClockId::kInvalidIndex)
	{
		StartHitStop(MakeId(index), durationSeconds);
	}
}

ClockId ClockRef::Get() const
{
	// 覚えた番号がまだ使えればそれ。初回か、時計が消された・作り直されたときだけ名前で探し直す
	const TimeManager& time = TimeManager::GetInstance();
	if (!time.IsValid(cached_))
	{
		const uint32_t index = time.FindIndexOrWarn(name_);
		cached_ = (index != ClockId::kInvalidIndex) ? time.MakeId(index) : ClockId{};
	}
	return cached_;
}

#ifdef USE_IMGUI
void TimeManager::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterSettingsPage(this, "システム", "タイムマネージャー", [this]() { this->DrawImGui(); });
}

void TimeManager::UnregisterDebugUI()
{
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
}

void TimeManager::DrawImGui()
{
	ImGui::Text("フレーム: %llu", static_cast<unsigned long long>(frameCount_));
	ImGui::TextDisabled("時計は親子でつながる。親を止めると子も止まり、子の速さは親の速さ×自分の倍率になる");
	DrawClockTree(kRealIndex);
}

void TimeManager::DrawClockTree(uint32_t index)
{
	Clock& clock = *clocks_[index];
	bool hasChildren = false;
	for (size_t i = index + 1; i < clocks_.size(); ++i)
	{
		if (clocks_[i]->alive && clocks_[i]->parent == index)
		{
			hasChildren = true;
			break;
		}
	}

	ImGui::PushID(static_cast<int>(index));
	const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | (hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf);
	if (ImGui::TreeNodeEx(clock.name.c_str(), flags))
	{
		if (index != kRealIndex)
		{
			ImGui::Checkbox("止める", &clock.paused);
			ImGui::SameLine();
			ImGui::SliderFloat("倍率", &clock.context.timeScale, 0.0f, kMaxTimeScaleSlider, "%.2f");
			if (clock.hitStopRemaining > 0.0f)
			{
				ImGui::Text("ヒットストップ 残り %.2f s", clock.hitStopRemaining);
			}
		}
		ImGui::TextDisabled("deltaTime %.4f / 倍率なし %.4f    累計 %.2f s / 倍率なし %.2f s",
			clock.context.deltaTime, clock.context.realDeltaTime, clock.context.gameTime, clock.context.realGameTime);

		for (size_t i = index + 1; i < clocks_.size(); ++i)
		{
			if (clocks_[i]->alive && clocks_[i]->parent == index)
			{
				DrawClockTree(static_cast<uint32_t>(i));
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}
#else
void TimeManager::RegisterDebugUI() {}
void TimeManager::UnregisterDebugUI() {}
#endif
} // namespace KCE
