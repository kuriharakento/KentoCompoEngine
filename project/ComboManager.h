#pragma once

class ComboManager
{
public:
    // --- シングルトンアクセス ---
    static ComboManager& Instance()
    {
        static ComboManager instance;
        return instance;
    }

    // --- コンボパラメータ ---
    static constexpr float kComboTimeout = 5.0f; // コンボの猶予時間（秒）

    // 敵を撃破したときに呼ぶ（複数同時撃破ならcount指定）
    void OnEnemyDefeated(int count = 1);

    // 毎フレーム呼び出す
    void Update();

    // 強制リセット（エリア遷移・死亡時など）
    void Reset();

    // --- ゲッター ---
    int GetComboCount() const { return comboCount_; }
    float GetComboTimer() const { return comboTimer_; }
    bool IsActive() const { return comboCount_ > 0; }

private:
    // --- 内部変数 ---
	// 最大コンボ数
    int maxComboCount_ = 0;
	// 現在のコンボ数
    int comboCount_ = 0;
	// コンボ猶予タイマー
    float comboTimer_ = 0.0f;

    // --- シングルトン禁止処理 ---
    ComboManager() = default;
    ComboManager(const ComboManager&) = delete;
    ComboManager& operator=(const ComboManager&) = delete;
};