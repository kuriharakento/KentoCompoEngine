#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include "Timer.h"

class TimerManager
{
public:
    // シングルトンインスタンスを取得
    static TimerManager& GetInstance();

	// コンストラクタ
    TimerManager();

	// デストラクタ
    ~TimerManager();
   
    // タイマー追加
    void AddTimer(const std::string& name, float duration, DeltaTimeType deltaType = DeltaTimeType::DeltaTime);
    void AddTimer(std::unique_ptr<Timer> timer);

    // タイマー取得
    Timer* GetTimer(const std::string& name);

    // 毎フレーム呼び出し
    void Update();

    // すべてのタイマーをクリア
    void Clear();

    // タイマーが存在するか
    bool HasTimer(const std::string& name) const;

private:
	// シングルトンインスタンス
    
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    TimerManager(TimerManager&&) = delete;
    TimerManager& operator=(TimerManager&&) = delete;

    std::unordered_map<std::string, std::unique_ptr<Timer>> timers_;
};