#include "CinematicLetterbox.h"
#include "time/TimeManager.h"
#include "imgui/imgui.h"
#include <algorithm>

CinematicLetterbox::CinematicLetterbox() {}
CinematicLetterbox::~CinematicLetterbox() {}

void CinematicLetterbox::Initialize(SpriteCommon* spriteCommon, const std::string& texturePath, float screenWidth, float screenHeight)
{
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;

    // 荳企Κ縺ｮ繝舌・菴懈・・医が繝ｼ繝舌・繧ｷ繝･繝ｼ繝亥ｯｾ蠢懊・縺溘ａ菴咏區繧貞性繧√※螟ｧ縺阪ａ縺ｫ菴懈・・・
    topBar_ = std::make_unique<Sprite>();
    topBar_->Initialize(spriteCommon, texturePath);
    topBar_->SetSize({ screenWidth_, letterboxHeight_ + overshootMargin_ });
    topBar_->SetAnchorPoint({ 0.0f, 1.0f }); // 荳狗ｫｯ繧貞渕貅悶↓縺吶ｋ縺薙→縺ｧ縲∫判髱｢荳企Κ縺九ｉ髯阪ｊ縺ｦ縺上ｋ蜍輔″繧貞ｮ溽樟
    topBar_->SetColor(color_);
    topBar_->SetPosition({ 0.0f, 0.0f });

    // 荳矩Κ縺ｮ繝舌・菴懈・・医が繝ｼ繝舌・繧ｷ繝･繝ｼ繝亥ｯｾ蠢懊・縺溘ａ菴咏區繧貞性繧√※螟ｧ縺阪ａ縺ｫ菴懈・・・
    bottomBar_ = std::make_unique<Sprite>();
    bottomBar_->Initialize(spriteCommon, texturePath);
    bottomBar_->SetSize({ screenWidth_, letterboxHeight_ + overshootMargin_ });
    bottomBar_->SetAnchorPoint({ 0.0f, 0.0f }); // 荳顔ｫｯ繧貞渕貅悶↓縺吶ｋ縺薙→縺ｧ縲∫判髱｢荳矩Κ縺九ｉ荳翫′縺｣縺ｦ縺上ｋ蜍輔″繧貞ｮ溽樟
    bottomBar_->SetColor(color_);
    bottomBar_->SetPosition({ 0.0f, screenHeight_ });
}

void CinematicLetterbox::Show(float duration)
{
    // 譌｢縺ｫ陦ｨ遉ｺ荳ｭ縺ｾ縺溘・陦ｨ遉ｺ貂医∩縺ｮ蝣ｴ蜷医・譌ｩ譛溘Μ繧ｿ繝ｼ繝ｳ
    if (state_ == LetterboxState::Visible || state_ == LetterboxState::Showing)
        return;

    duration_ = duration;
    elapsed_ = 0.0f;
    state_ = LetterboxState::Showing;
}

void CinematicLetterbox::Hide(float duration)
{
    // 譌｢縺ｫ髱櫁｡ｨ遉ｺ荳ｭ縺ｾ縺溘・髱櫁｡ｨ遉ｺ貂医∩縺ｮ蝣ｴ蜷医・譌ｩ譛溘Μ繧ｿ繝ｼ繝ｳ
    if (state_ == LetterboxState::Hidden || state_ == LetterboxState::Hiding)
        return;

    duration_ = duration;
    elapsed_ = 0.0f;
    state_ = LetterboxState::Hiding;
}

void CinematicLetterbox::Update()
{
	// ImGui陦ｨ遉ｺ
    ShowImGui();

    float deltaTime = TimeManager::GetInstance().GetUIContext().deltaTime;

    // 陦ｨ遉ｺ繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ蜃ｦ逅・
    if (state_ == LetterboxState::Showing)
    {
        elapsed_ += deltaTime;
        float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
        progress_ = ApplyEasing(t);

        // 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ螳御ｺ・愛螳・
        if (t >= 1.0f)
        {
            state_ = LetterboxState::Visible;
            progress_ = 1.0f;
        }

        UpdateBarPositions();
    }
    // 髱櫁｡ｨ遉ｺ繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ蜃ｦ逅・
    else if (state_ == LetterboxState::Hiding)
    {
        elapsed_ += deltaTime;
        float t = std::clamp(elapsed_ / duration_, 0.0f, 1.0f);
        progress_ = 1.0f - ApplyEasing(t); // 騾・婿蜷代・繧､繝ｼ繧ｸ繝ｳ繧ｰ驕ｩ逕ｨ

        // 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ螳御ｺ・愛螳・
        if (t >= 1.0f)
        {
            state_ = LetterboxState::Hidden;
            progress_ = 0.0f;
        }

        UpdateBarPositions();
    }
}

void CinematicLetterbox::Draw()
{
    // 螳悟・縺ｫ髱櫁｡ｨ遉ｺ縺ｮ蝣ｴ蜷医・謠冗判繧ｳ繧ｹ繝医ｒ蜑頑ｸ帙☆繧九◆繧∵掠譛溘Μ繧ｿ繝ｼ繝ｳ
    if (state_ == LetterboxState::Hidden && progress_ <= 0.0f)
        return;

    if (topBar_) topBar_->Draw();
    if (bottomBar_) bottomBar_->Draw();
}

void CinematicLetterbox::SetColor(const Vector4& color)
{
    color_ = color;
    if (topBar_) topBar_->SetColor(color_);
    if (bottomBar_) bottomBar_->SetColor(color_);
}

void CinematicLetterbox::SetLetterboxHeight(float height)
{
    letterboxHeight_ = height;
    UpdateBarSizes();
    UpdateBarPositions();
}

void CinematicLetterbox::UpdateBarSizes()
{
    if (!topBar_ || !bottomBar_) return;

    // 繧ｪ繝ｼ繝舌・繧ｷ繝･繝ｼ繝亥・繧貞性繧√◆繧ｵ繧､繧ｺ縺ｫ譖ｴ譁ｰ
    topBar_->SetSize({ screenWidth_, letterboxHeight_ + overshootMargin_ });
    bottomBar_->SetSize({ screenWidth_, letterboxHeight_ + overshootMargin_ });
}

void CinematicLetterbox::UpdateBarPositions()
{
    if (!topBar_ || !bottomBar_) return;

    // 荳企Κ縺ｮ繝舌・菴咲ｽｮ繧定ｨ育ｮ暦ｼ育判髱｢荳顔ｫｯ縺九ｉ騾ｲ陦悟ｺｦ縺ｫ蠢懊§縺ｦ髯阪ｊ縺ｦ縺上ｋ・・
    float topY = letterboxHeight_ * progress_;
    topBar_->SetPosition({ 0.0f, topY });

    // 荳矩Κ縺ｮ繝舌・菴咲ｽｮ繧定ｨ育ｮ暦ｼ育判髱｢荳狗ｫｯ縺九ｉ騾ｲ陦悟ｺｦ縺ｫ蠢懊§縺ｦ荳翫′縺｣縺ｦ縺上ｋ・・
    float bottomY = screenHeight_ - (letterboxHeight_ * progress_);
    bottomBar_->SetPosition({ 0.0f, bottomY });

    // 繧ｹ繝励Λ繧､繝医・螟画鋤陦悟・繧呈峩譁ｰ
	topBar_->Update();
	bottomBar_->Update();
}

float CinematicLetterbox::ApplyEasing(float t) const
{
    // 險ｭ螳壹＆繧後◆繧､繝ｼ繧ｸ繝ｳ繧ｰ繧ｿ繧､繝励↓蠢懊§縺溯｣憺俣髢｢謨ｰ繧帝←逕ｨ
    switch (easeType_)
    {
    case LetterboxEase::Linear:
        return t;
    case LetterboxEase::InSine:
        return EaseInSine(t);
    case LetterboxEase::OutSine:
        return EaseOutSine(t);
    case LetterboxEase::InOutSine:
        return EaseInOutSine(t);
    case LetterboxEase::InQuint:
        return EaseInQuint(t);
    case LetterboxEase::OutQuint:
        return EaseOutQuint(t);
    case LetterboxEase::InOutQuint:
        return EaseInOutQuint(t);
    case LetterboxEase::InCirc:
        return EaseInCirc(t);
    case LetterboxEase::OutCirc:
        return EaseOutCirc(t);
    case LetterboxEase::InOutCirc:
        return EaseInOutCirc(t);
    case LetterboxEase::InElastic:
        return EaseInElastic(t);
    case LetterboxEase::OutElastic:
        return EaseOutElastic(t);
    case LetterboxEase::InOutElastic:
        return EaseInOutElastic(t);
    case LetterboxEase::InExpo:
        return EaseInExpo(t);
    case LetterboxEase::OutExpo:
        return EaseOutExpo(t);
    case LetterboxEase::InOutExpo:
        return EaseInOutExpo(t);
    case LetterboxEase::OutQuad:
        return EaseOutQuad(t);
    case LetterboxEase::InOutQuart:
        return EaseInOutQuart(t);
    case LetterboxEase::InBack:
        return EaseInBack(t);
    case LetterboxEase::OutBack:
        return EaseOutBack(t);
    case LetterboxEase::InOutBack:
        return EaseInOutBack(t);
    case LetterboxEase::OutBounce:
        return EaseOutBounce(t);
    case LetterboxEase::InBounce:
        return EaseInBounce(t);
    case LetterboxEase::InOutBounce:
        return EaseInOutBounce(t);
    default:
        return t;
    }
}

void CinematicLetterbox::ShowImGui()
{
#ifdef USE_IMGUI
    if (ImGui::Begin("Cinematic Letterbox"))
    {
        // 迥ｶ諷玖｡ｨ遉ｺ
        const char* stateNames[] = { "Hidden", "Showing", "Visible", "Hiding" };
        ImGui::Text("State: %s", stateNames[static_cast<int>(state_)]);
        ImGui::Text("Progress: %.2f", progress_);
        ImGui::Text("Elapsed: %.2f / %.2f", elapsed_, duration_);

        // 菴咲ｽｮ繝・ヰ繝・げ
        if (topBar_)
        {
            Vector2 topPos = topBar_->GetPosition();
            Vector2 topSize = topBar_->GetSize();
            ImGui::Text("Top Bar Y: %.2f (Size: %.2f)", topPos.y, topSize.y);
        }
        if (bottomBar_)
        {
            Vector2 bottomPos = bottomBar_->GetPosition();
            Vector2 bottomSize = bottomBar_->GetSize();
            ImGui::Text("Bottom Bar Y: %.2f (Size: %.2f)", bottomPos.y, bottomSize.y);
        }

        // 繝代Λ繝｡繝ｼ繧ｿ隱ｿ謨ｴ
        ImGui::Separator();
        if (ImGui::DragFloat("Letterbox Height", &letterboxHeight_, 1.0f, 50.0f, 300.0f))
        {
            SetLetterboxHeight(letterboxHeight_);
        }
        if (ImGui::DragFloat("Overshoot Margin", &overshootMargin_, 1.0f, 50.0f, 300.0f))
        {
            UpdateBarSizes();
        }
        ImGui::DragFloat("Duration", &duration_, 0.1f, 0.1f, 5.0f);

        // 繧､繝ｼ繧ｸ繝ｳ繧ｰ繧ｿ繧､繝鈴∈謚・
        const char* easeNames[] = {
            "Linear", "InSine", "OutSine", "InOutSine",
            "InQuint", "OutQuint", "InOutQuint",
            "InCirc", "OutCirc", "InOutCirc",
            "InElastic", "OutElastic", "InOutElastic",
            "InExpo", "OutExpo", "InOutExpo",
            "OutQuad", "InOutQuart",
            "InBack", "OutBack", "InOutBack",
            "OutBounce", "InBounce", "InOutBounce"
        };
        int currentEase = static_cast<int>(easeType_);
        if (ImGui::Combo("Ease Type", &currentEase, easeNames, IM_ARRAYSIZE(easeNames)))
        {
            easeType_ = static_cast<LetterboxEase>(currentEase);
        }

        // 濶ｲ險ｭ螳・
        float color[4] = { color_.x, color_.y, color_.z, color_.w };
        if (ImGui::ColorEdit4("Color", color))
        {
            SetColor({ color[0], color[1], color[2], color[3] });
        }

        // 繧ｳ繝ｳ繝医Ο繝ｼ繝ｫ繝懊ち繝ｳ
        ImGui::Separator();
        if (ImGui::Button("Show"))
        {
            Show(duration_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Hide"))
        {
            Hide(duration_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Toggle"))
        {
            if (state_ == LetterboxState::Hidden || state_ == LetterboxState::Hiding)
            {
                Show(duration_);
            }
            else
            {
                Hide(duration_);
            }
        }
    }
    ImGui::End();
#endif
}

