#pragma once

class Player;

class CarnageMode
{
public:
	CarnageMode() = default;
	// 初期化
	void Initialize(Player* player);
    // 更新
    void Update(float deltaTime);
    // 描画
	void Draw();

    // 開始・終了
    void Start();
    void End();

    // 状態参照
    bool IsActive() const { return isActive_; }
    float GetTimeLeft() const { return carnageTimer_; }
    int GetComboThreshold() const { return comboThreshold_; }

private:
    Player* player_;            // 管理対象プレイヤー
    bool isActive_ = false;     // カーネージモード中か
    float carnageTimer_ = 0.0f; // 残り時間
    int prevComboCount_ = 0;    // 前フレームのコンボ数

    // 設定値
    const float initialTime_ = 8.0f;      // 初期時間
    const float extensionTime_ = 1.0f;    // コンボ増加ごとの延長秒数
    const int comboThreshold_ = 10;       // 発動コンボ数

    // バフ・演出
    void ApplyBuffs();
    void RemoveBuffs();
    void ShowUI();
    void HideUI();
};