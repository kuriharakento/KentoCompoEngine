#include "SkillSelectionUI.h"
#include "application/UI/GameUI.h"
#include "graphics/2d/FontSprite.h"
#include "input/Input.h"
#include "base/WinApp.h"
#include "math/MathUtils.h"
#include "engine/time/TimeManager.h"


void SkillSelectionUI::Initialize(SpriteCommon* spriteCommon)
{
	spriteCommon_ = spriteCommon;

	// 半透明黒のオーバーレイ
	overlay_ = std::make_unique<GameUI>();
	overlay_->Initialize(spriteCommon, "./Resources/black.png");
	overlay_->SetAnchorPoint({ 0.0f, 0.0f });
	overlay_->SetScreenPosition({ 0.0f, 0.0f });
	overlay_->SetSize({ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
	overlay_->SetColor({ 0.0f, 0.0f, 0.0f, 0.7f });
	overlay_->SetVisible(false);
	overlay_->SetInteractable(false);

	mainTitleFont_ = std::make_unique<FontSprite>();
	mainTitleFont_->Initialize(spriteCommon, "nico");
	mainTitleFont_->SetVisible(false);
}

void SkillSelectionUI::Update()
{
	if (!isActive_) return;

	float dt = TimeManager::GetInstance().GetGameContext().deltaTime;
	if (inputLockoutTimer_ > 0.0f) {
		inputLockoutTimer_ -= dt;
	}

	auto* input = Input::GetInstance();

	for (size_t i = 0; i < cards_.size(); ++i)
	{
		auto& card = cards_[i];
		if (card.bg) card.bg->Update();

		// ホバー演出
		float targetScale = (card.bg && card.bg->IsHovered()) ? kHoverScale : kNormalScale;
		card.scale += (targetScale - card.scale) * kScaleLerpSpeed;
		
		if (card.bg)
		{
			card.bg->SetSize({ kCardWidth * card.scale, kCardHeight * card.scale });
			// 明度変更
			float brightness = (card.bg->IsHovered()) ? 1.0f : 0.85f;
			card.bg->SetColor({ 0.2f * brightness, 0.3f * brightness, 0.5f * brightness, 0.95f });
		}

		// 入力判定 (1, 2, 3キー または クリック)
		// 表示直後は誤操作防止のため入力を受け付けない
		if (inputLockoutTimer_ <= 0.0f)
		{
			bool keyTriggered = false;
			if (i == 0 && input->TriggerKey(DIK_1)) keyTriggered = true;
			if (i == 1 && input->TriggerKey(DIK_2)) keyTriggered = true;
			if (i == 2 && input->TriggerKey(DIK_3)) keyTriggered = true;

			if (keyTriggered || (card.bg && card.bg->IsClicked()))
			{
				auto cb = card.onSelect;
				HideAll();
				if (cb) cb();
				break;
			}
		}
	}
}

void SkillSelectionUI::Draw()
{
	if (!isActive_) return;

	if (overlay_) overlay_->Draw();

	float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;

	// タイトル
	if (mainTitleFont_)
	{
		mainTitleFont_->SetColor({ 1.0f, 1.0f, 0.4f, 1.0f });
		mainTitleFont_->DrawText("UPGRADE SELECT", { centerX - 250.0f, kCardY - kCardHeight * 0.5f - 80.0f }, kMainTitleScale);
	}

	for (auto& card : cards_)
	{
		if (card.bg) card.bg->Draw();
		
		Vector2 pos = card.bg->GetScreenPosition();

		// タイトル
		if (card.titleFont)
		{
			card.titleFont->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			float titleX = pos.x - kCardWidth * 0.35f;
			float titleY = pos.y - kCardHeight * 0.3f;
			card.titleFont->DrawText(card.title, { titleX, titleY }, kCardTitleScale);
		}

		// 説明文
		if (card.descFont)
		{
			card.descFont->SetColor({ 0.8f, 0.9f, 1.0f, 1.0f });
			float descX = pos.x - kCardWidth * 0.42f;
			float descY = pos.y - kCardHeight * 0.1f;
			card.descFont->DrawText(card.desc, { descX, descY }, kCardDescScale);
		}
	}
}

void SkillSelectionUI::Show(uint32_t level, const std::string& optionA, const std::string& optionB,
	std::function<void(int)> onSelect)
{
    std::vector<UpgradeOption> options;
    options.push_back({ optionA, "Unlock this skill route.", [onSelect](){ onSelect(0); } });
    options.push_back({ optionB, "Unlock this skill route.", [onSelect](){ onSelect(1); } });
    
    ShowUpgrades(options);
    
    // 特別なタイトル上書き
    if (mainTitleFont_) {
        // level 2 or 3
    }
}

void SkillSelectionUI::ShowUpgrades(const std::vector<UpgradeOption>& options)
{
	isActive_ = true;
	cards_.clear();

	float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	size_t count = options.size();
	float totalWidth = (kCardWidth * count) + (kCardGap * (count - 1));
	float startX = centerX - totalWidth * 0.5f + kCardWidth * 0.5f;

	for (size_t i = 0; i < count; ++i)
	{
		CardInstance card;
		card.title = options[i].title;
		card.desc = options[i].description;
		card.onSelect = options[i].onSelect;

		card.bg = std::make_unique<GameUI>();
		card.bg->Initialize(spriteCommon_, "./Resources/white1x1.png");
		card.bg->SetAnchorPoint({ 0.5f, 0.5f });
		card.bg->SetScreenPosition({ startX + i * (kCardWidth + kCardGap), kCardY });
		card.bg->SetSize({ kCardWidth, kCardHeight });
		card.bg->SetVisible(true);
		card.bg->SetInteractable(true);

		card.titleFont = std::make_unique<FontSprite>();
		card.titleFont->Initialize(spriteCommon_, "nico");
		card.titleFont->SetVisible(true);

		card.descFont = std::make_unique<FontSprite>();
		card.descFont->Initialize(spriteCommon_, "nico");
		card.descFont->SetVisible(true);

		cards_.push_back(std::move(card));
	}

	if (overlay_) overlay_->SetVisible(true);
	if (mainTitleFont_) mainTitleFont_->SetVisible(true);

	// 表示直後の誤操作防止タイマーをセット
	inputLockoutTimer_ = kInputLockoutTime;
}

void SkillSelectionUI::HideAll()
{
	isActive_ = false;
	cards_.clear();
	if (overlay_) overlay_->SetVisible(false);
	if (mainTitleFont_) mainTitleFont_->SetVisible(false);
}
