#pragma once
#include "graphics/2d/NumberSprite.h"

class ComboManager
{
public:
    // --- シングルトンアクセス ---
    static ComboManager& GetInstance()
    {
        static ComboManager instance;
        return instance;
    }

    void Initialize(SpriteCommon* spriteCommon);

    // --- コンボパラメータ ---
    static constexpr float kComboTimeout = 5.0f; // コンボの猶予時間（秒）

    // 敵を撃破したときに呼ぶ
    void OnEnemyDefeated(int count = 1);

    // 更新
    void Update();

    // 描画
    void Draw();

    // 強制リセット（エリア遷移・死亡時など）
    void Reset();

    // --- ゲッター ---
    int GetComboCount() const { return comboCount_; }
    float GetComboTimer() const { return comboTimer_; }
    bool IsActive() const { return comboCount_ > 0; }

private:
    void DrawImGUi();

private:
    // --- 内部変数 ---
	// 最大コンボ数
    int maxComboCount_ = 0;
	// 現在のコンボ数
    int comboCount_ = 0;
	// コンボ猶予タイマー
    float comboTimer_ = 0.0f;

	NumberSprite comboNumberSprite_; // コンボ数表示用スプライト

private: // シングルトン
    ComboManager() = default;
    ComboManager(const ComboManager&) = delete;
    ComboManager& operator=(const ComboManager&) = delete;
};
