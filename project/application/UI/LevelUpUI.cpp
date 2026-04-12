#include "LevelUpUI.h"
#include "engine/time/TimeManager.h"
#include "math/MatrixFunc.h"
#include <algorithm>
#include <cmath>

void LevelUpUI::Initialize(SpriteCommon* spriteCommon)
{
    // 暫定テクスチャ uvChecker.png
    GameUI::Initialize(spriteCommon, "./Resources/uvChecker.png");

    SetAnchorPoint({ 0.5f, 0.5f });
    
    // 画面中央。30%ほどの高さに配置
    SetScreenPosition({ static_cast<float>(WinApp::kClientWidth) * 0.5f, static_cast<float>(WinApp::kClientHeight) * 0.35f });
    
    // 幅広のバナー状にする
    SetSize({ 600.0f, 120.0f });
    
    // 初期状態は非表示
    SetVisible(false);
    SetInteractable(false);
    isActive_ = false;
    timer_ = 0.0f;
}

void LevelUpUI::Update()
{
    if (!isActive_) return;

    float dt = TimeManager::GetInstance().GetGameContext().deltaTime;
    timer_ -= dt;

    if (timer_ <= 0.0f)
    {
        isActive_ = false;
        SetVisible(false);
    }
    else
    {
        // アニメーション計算 (progress 0.0 -> 1.0)
        float progress = 1.0f - (timer_ / kDuration);

        // 拡大しながらフェードアウト
        float scale = 1.0f + std::sin(progress * 3.14159f) * 0.2f;
        
        // 前半はハッキリ、後半から消える
        float alpha = 1.0f;
        if (progress > 0.5f)
        {
            alpha = (1.0f - progress) * 2.0f;
        }

        SetSize({ 600.0f * scale, 120.0f * scale });
        // レベルアップらしく黄金色っぽく (暫定)
        SetColor({ 1.0f, 0.9f, 0.2f, alpha });
    }

    GameUI::Update();
}

void LevelUpUI::Trigger()
{
    timer_ = kDuration;
    isActive_ = true;
    SetVisible(true);
}
