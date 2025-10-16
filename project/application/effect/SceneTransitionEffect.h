#pragma once
#include <vector>
#include <memory>
#include <string>
#include "graphics/2d/Sprite.h"
#include "math/Easing.h"

// フェードモード
enum class TransitionMode
{
    LeftTopToRightBottom,
    RightBottomToLeftTop,
    RightTopToLeftBottom,
    LeftBottomToRightTop,
    TopToBottom,
    BottomToTop,
    CenterToEdges,
    EdgesToCenter
};

enum class FadeType
{
    FadeIn,
    FadeOut
};

enum class SceneTransitionEase
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

enum class TransitionState { Idle, Playing, Done };

class SceneTransitionEffect
{
public:
    SceneTransitionEffect();
    ~SceneTransitionEffect();

    void Initialize(SpriteCommon* spriteCommon, const std::string& texturePath, int gridX, int gridY, float screenWidth, float screenHeight);

    // グラデーションカラー付き演出開始
    void Start(float duration, const Vector4& startColor, const Vector4& endColor);

    void Update();
    void Draw();

    TransitionState GetState() const;
    void SetEaseType(SceneTransitionEase type);
    void SetMode(TransitionMode mode);
    void SetFadeType(FadeType type);

    // ImGui操作用
    void ShowImGui();

private:
    float ApplyEasing(float t) const;
    Vector4 LerpColor(const Vector4& c0, const Vector4& c1, float t) const;
    float CalcGridProgress(int x, int y) const;

    int gridX_ = 6;
    int gridY_ = 4;
    float screenWidth_ = 1280.0f;
    float screenHeight_ = 720.0f;
    float transitionRate_ = 0.0f;
    SceneTransitionEase easeType_ = SceneTransitionEase::Linear;
    float duration_ = 1.0f;
    float elapsed_ = 0.0f;
    TransitionState state_ = TransitionState::Idle;
    Vector4 startColor_ = { 1.0f,1.0f,1.0f,1.0f };
    Vector4 endColor_ = { 1.0f,1.0f,1.0f,1.0f };
    TransitionMode mode_ = TransitionMode::LeftTopToRightBottom;
    FadeType fadeType_ = FadeType::FadeOut;

    std::vector<std::vector<std::unique_ptr<Sprite>>> gridSprites_;
};