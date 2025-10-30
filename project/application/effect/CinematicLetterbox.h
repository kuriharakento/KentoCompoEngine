#pragma once
#include <memory>
#include "graphics/2d/Sprite.h"
#include "math/Easing.h"

// イージングタイプ
enum class LetterboxEase
{
    Linear,
    InSine,
    OutSine,
    InOutSine,
    InQuint,
    OutQuint,
    InOutQuint,
    InCirc,
    OutCirc,
    InOutCirc,
    InElastic,
    OutElastic,
    InOutElastic,
    InExpo,
    OutExpo,
    InOutExpo,
    OutQuad,
    InOutQuart,
    InBack,
    OutBack,
    InOutBack,
    OutBounce,
    InBounce,
    InOutBounce
};

// レターボックスの状態
enum class LetterboxState
{
    Hidden,    // 完全に隠れている
    Showing,   // 表示中
    Visible,   // 完全に表示されている
    Hiding     // 非表示中
};

class CinematicLetterbox
{
public:
    CinematicLetterbox();
    ~CinematicLetterbox();

    // 初期化
    void Initialize(SpriteCommon* spriteCommon, const std::string& texturePath, float screenWidth, float screenHeight);

    // レターボックスを表示
    void Show(float duration = 1.0f);

    // レターボックスを非表示
    void Hide(float duration = 1.0f);

    // 更新
    void Update();

    // 描画
    void Draw();

    // 状態取得
    LetterboxState GetState() const { return state_; }

    // パラメータ設定
    void SetEaseType(LetterboxEase type) { easeType_ = type; }
    void SetLetterboxHeight(float height);
    void SetColor(const Vector4& color);

    // ImGui操作用
    void ShowImGui();

private:
    float ApplyEasing(float t) const;
    void UpdateBarPositions();
    void UpdateBarSizes();

private:
    // 画面サイズ
    float screenWidth_ = 1280.0f;
    float screenHeight_ = 720.0f;

    // レターボックスの高さ（画面の上下それぞれ）
    float letterboxHeight_ = 100.0f;

    // オーバーシュート用の余白（BackやElasticなどのイージングで範囲外に出る分）
    float overshootMargin_ = 100.0f;

    // アニメーション関連
    float duration_ = 1.0f;
    float elapsed_ = 0.0f;
    float progress_ = 0.0f; // 0.0 = 完全に隠れている, 1.0 = 完全に表示

    // 状態
    LetterboxState state_ = LetterboxState::Hidden;
    LetterboxEase easeType_ = LetterboxEase::InOutBack;

    // 色
    Vector4 color_ = { 0.0f, 0.0f, 0.0f, 1.0f }; // デフォルトは黒

    // スプライト
    std::unique_ptr<Sprite> topBar_;
    std::unique_ptr<Sprite> bottomBar_;
};