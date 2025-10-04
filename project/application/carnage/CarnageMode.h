#pragma once
#include <memory>

#include "application/effect/CarnageModeEffect.h"
#include "time/Timer.h"

class Player;

class CarnageMode
{
public:
    // 初期化
    CarnageMode(Player* player);

    // 更新
    void Update();

    // コンボ数監視して条件達成なら開始
    void TryStart();

    // タイマー延長
    void ExtendTimer();

    bool IsActive() const;
    float GetTimeLeft() const;

private: // メンバ関数
	// バフ適用/解除
    void ApplyBuffs();
    void RemoveBuffs();
	// UI表示
    void ShowUI();
    void HideUI();
    void ImGui();

private: // メンバ変数
    // プレイヤーのポインタ
    Player* player_;

    // エフェクト
	std::unique_ptr<CarnageModeEffect> effect_;

    // カーネージモード用タイマー
    std::unique_ptr<Timer> timer_;

    // カーネージモード発動に必要なコンボ数
    const int comboThreshold_ = 10;
    // カーネージモード初期時間
    const float initialTime_ = 8.0f;
    // コンボ増加ごとのタイマー延長時間
    const float extensionTime_ = 1.0f;

    // 攻撃力上昇率（例：0.5f → 50%アップ）
    float attackUpRate_ = 0.5f;
	// 移動速度上昇率
	float speedUpRate_ = 1.0f;
};