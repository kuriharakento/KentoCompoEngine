#include "TimeManager.h"

#include "imgui/imgui.h"

TimeManager& TimeManager::GetInstance()
{
	// 静的ローカル変数でシングルトンを実現
	static TimeManager instance;
	return instance;
}

TimeManager::TimeManager()
{
	// 初回時刻を記録
	lastUpdate_ = std::chrono::steady_clock::now();
	// 各コンテキストの初期化（deltaTime, gameTime, realDeltaTime, realGameTime, timeScale）
	gameContext_ = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	uiContext_ = { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
}

void TimeManager::UpdateTimeContext(TimeContext& context, float realDelta, bool isPaused)
{
	// 実時間の設定
	context.realDeltaTime = realDelta;

	// ポーズ時はスケール適用時間を0にする
	context.deltaTime = isPaused ? 0.0f : realDelta * context.timeScale;

	// 累積時間の更新
	context.gameTime += context.deltaTime;
	context.realGameTime += context.realDeltaTime;
}

void TimeManager::Update()
{
#ifdef USE_IMGUI
	// ImGuiによるデバッグUI表示
	ImGui::Begin("Time Manager");

	// 全体のポーズ設定
	ImGui::Checkbox("Paused", &paused_);
	ImGui::Separator();

	// ゲームコンテキストの表示
	ImGui::Text("Game Context:");
	ImGui::SliderFloat("Game Time Scale", &gameContext_.timeScale, 0.0f, 3.0f, "%.2f");
	ImGui::Text("  GameTime: %.2f", gameContext_.gameTime);
	ImGui::Text("  RealGameTime: %.2f", gameContext_.realGameTime);
	ImGui::Text("  DeltaTime: %.4f", gameContext_.deltaTime);
	ImGui::Text("  RealDeltaTime: %.4f", gameContext_.realDeltaTime);

	ImGui::Separator();

	// UIコンテキストの表示
	ImGui::Text("UI Context:");
	ImGui::SliderFloat("UI Time Scale", &uiContext_.timeScale, 0.0f, 3.0f, "%.2f");
	ImGui::Text("  GameTime: %.2f", uiContext_.gameTime);
	ImGui::Text("  RealGameTime: %.2f", uiContext_.realGameTime);
	ImGui::Text("  DeltaTime: %.4f", uiContext_.deltaTime);
	ImGui::Text("  RealDeltaTime: %.4f", uiContext_.realDeltaTime);

	ImGui::End();
#endif

	// 実時間の計測
	auto now = std::chrono::steady_clock::now();
	float realDelta = std::chrono::duration<float>(now - lastUpdate_).count();
	lastUpdate_ = now;

	// 各コンテキストの更新
	UpdateTimeContext(gameContext_, realDelta, paused_);
	UpdateTimeContext(uiContext_, realDelta, false);      // UIは通常ポーズの影響を受けない
}

void TimeManager::Pause()
{
	// ポーズ状態に設定
	paused_ = true;
}

void TimeManager::Resume()
{
	// ポーズ解除
	paused_ = false;
}

bool TimeManager::IsPaused() const
{
	return paused_;
}
