#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include "Timer.h"

/**
 * @brief タイマー管理クラス
 * 
 * 複数のタイマーを一元管理し、ライフサイクル（追加、更新、削除）を制御する。
 * タイマーは名前で識別され、終了したタイマーは自動的に削除される。
 * シングルトンパターンで実装され、ゲーム全体で共有される。
 */
class TimerManager
{
public:
    /**
     * @brief シングルトンインスタンスを取得
     * @return TimerManagerのインスタンス参照
     */
    static TimerManager& GetInstance();

    /**
     * @brief コンストラクタ
     */
    TimerManager();

    /**
     * @brief デストラクタ
     */
    ~TimerManager();
   
    // --- タイマー追加 ---

    /**
     * @brief 新しいタイマーを追加（パラメータ指定）
     * @param name タイマーの識別名
     * @param duration 継続時間（秒）
     * @param deltaType 使用するデルタタイムのタイプ
     */
    void AddTimer(const std::string& name, float duration, DeltaTimeType deltaType = DeltaTimeType::DeltaTime);

    /**
     * @brief 既存のタイマーオブジェクトを追加
     * @param timer 追加するタイマー（所有権を移譲）
     */
    void AddTimer(std::unique_ptr<Timer> timer);

    // --- タイマー取得 ---

    /**
     * @brief タイマーを名前で取得
     * @param name タイマーの識別名
     * @return タイマーへのポインタ（存在しない場合はnullptr）
     */
    Timer* GetTimer(const std::string& name);

    /**
     * @brief 毎フレーム呼び出す更新関数
     * 
     * 全タイマーを更新し、終了したタイマーを自動削除する。
     */
    void Update();

    /**
     * @brief すべてのタイマーをクリア
     */
    void Clear();

    /**
     * @brief 指定した名前のタイマーを削除
     * @param name 削除するタイマーの名前
     */
    void RemoveTimer(const std::string& name);

    /**
     * @brief タイマーが存在するかを確認
     * @param name タイマーの識別名
     * @return 存在する場合はtrue
     */
    bool HasTimer(const std::string& name) const;

#ifdef USE_IMGUI
    /** @brief DebugUIManager 経由で描画されるデバッグ情報 */
    void DrawImGui();
#endif

private:
    // シングルトンパターンのためコピー・ムーブを禁止
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    TimerManager(TimerManager&&) = delete;
    TimerManager& operator=(TimerManager&&) = delete;

    std::unordered_map<std::string, std::unique_ptr<Timer>> timers_; // タイマー格納用マップ
};