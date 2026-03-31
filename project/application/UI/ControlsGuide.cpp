#include "ControlsGuide.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "math/VectorColorCodes.h"
#include "manager/graphics/TextureManager.h"
#include <algorithm>

void ControlsGuide::Initialize(SpriteCommon* spriteCommon, Registry* registry, EntityID playerEntity)
{
	spriteCommon_ = spriteCommon;
	registry_ = registry;
	playerEntity_ = playerEntity;

	// プロンプトをクリア
	prompts_.clear();

	// スキルの順に追加
	AddPrompt("LMB", "./Resources/UI/button/LMB.png", "./Resources/UI/text/LMB.png");
	AddPrompt("RMB", "./Resources/UI/button/RMB.png", "./Resources/UI/text/RMB.png");
	AddPrompt("Q", "./Resources/UI/button/Q.png", "./Resources/UI/text/Q.png");
	AddPrompt("E", "./Resources/UI/button/E.png", "./Resources/UI/text/E.png");

	// HPバーの初期化
	// 中身
	hpBarFG_ = std::make_unique<Sprite>();
	hpBarFG_->Initialize(spriteCommon_, "./Resources/UI/hp_bar_fill.png");
	hpBarFG_->SetPosition({ kHpBarX, kHpBarY });
	hpBarFG_->SetSize({ kHpBarWidth, kHpBarHeight });
	hpBarFG_->SetColor(VectorColorCodes::Lime);
	hpBarFG_->SetAnchorPoint({ 0.0f, 0.5f });

	// フレーム
	hpBarFrame_ = std::make_unique<Sprite>();
	hpBarFrame_->Initialize(spriteCommon_, "./Resources/UI/hp_bar_frame.png");
	hpBarFrame_->SetPosition({ kHpBarX - 4.0f, kHpBarY });
	hpBarFrame_->SetSize({ kHpBarWidth + 8.0f, kHpBarHeight + 8.0f });
	hpBarFrame_->SetColor({ 1,1,1,1 });
	hpBarFrame_->SetAnchorPoint({ 0.0f, 0.5f });
}

void ControlsGuide::Update()
{
	if (!isVisible_ || !registry_ || playerEntity_ == kInvalidEntity) return;

	// --- スキル情報の更新 ---
	if (registry_->HasComponent<SkillComponent>(playerEntity_))
	{
		auto& skill = registry_->GetComponent<SkillComponent>(playerEntity_);

		for (auto& prompt : prompts_)
		{
			float timer = 0.0f;
			float maxTimer = 1.0f;
			bool unlocked = false;

			if (prompt.actionName_ == "LMB") { timer = skill.lmbTimer_; maxTimer = SkillComponent::kLmbCooldown; unlocked = skill.isLmbUnlocked_; }
			else if (prompt.actionName_ == "RMB") { timer = skill.rmbTimer_; maxTimer = SkillComponent::kRmbCooldown; unlocked = skill.isRmbUnlocked_; }
			else if (prompt.actionName_ == "Q") { timer = skill.decoyTimer_; maxTimer = SkillComponent::kDecoyCooldown; unlocked = skill.isDecoyUnlocked_; }
			else if (prompt.actionName_ == "E") { timer = skill.impactTimer_; maxTimer = SkillComponent::kImpactCooldown; unlocked = skill.isImpactUnlocked_; }

			// クールタイム進捗 (0.0: 開始, 1.0: 完了)
			float progress = 1.0f;
			if (maxTimer > 0.0f)
			{
				progress = (std::max)(0.0f, (std::min)(1.0f, 1.0f - (timer / maxTimer)));
			}

			// テクスチャの元のサイズを取得
			const auto& metadata = TextureManager::GetInstance()->GetMetadata(prompt.iconPath_);
			float texW = static_cast<float>(metadata.width);
			float texH = static_cast<float>(metadata.height);

			// クールタイム演出 (リキッド・フィル)
			if (timer > 0.0f && unlocked)
			{
				// ベースは暗く
				prompt.icon_->SetColor({ 0.3f, 0.3f, 0.3f, 1.0f });

				// オーバーレイ（明るい）を下から満たしていく
				float currentH = 75.0f * progress;
				prompt.overlay_->SetSize({ 75.0f, currentH });
				
				// テクスチャ切り出し (下から progress 分)
				float cutH = texH * progress;
				prompt.overlay_->SetTextureLeftTop({ 0.0f, texH - cutH });
				prompt.overlay_->SetTextureSize({ texW, cutH });

				// Y座標を調整（下端を固定して上に伸ばす）
				// アンカーが 0.5, 0.5 なので、全体の中心から progress 分だけ下にずらした位置が中心になる
				float yOffset = (75.0f * 0.5f) * (1.0f - progress);
				prompt.overlay_->SetPosition({ kPromptBaseX, prompt.y_ + yOffset });
				prompt.overlay_->SetColor({ 1, 1, 1, 1 });
			}
			else
			{
				// クールタイム終了または未開放
				prompt.icon_->SetColor(unlocked ? VectorColorCodes::White : Vector4(0.2f, 0.2f, 0.2f, 1.0f));
				prompt.overlay_->SetColor({ 1, 1, 1, 0 }); // 非表示
			}

			prompt.icon_->Update();
			prompt.overlay_->Update();
			prompt.description_->Update();
		}
	}

	// --- HPバーの更新 ---
	if (registry_->HasComponent<ecs::StatusComponent>(playerEntity_))
	{
		auto& status = registry_->GetComponent<ecs::StatusComponent>(playerEntity_);
		float ratio = (std::max)(0.0f, (std::min)(1.0f, status.hp_.GetValue() / status.maxHp_.GetValue()));
		
		hpBarFG_->SetSize({ kHpBarWidth * ratio, kHpBarHeight });
		
		if (ratio < 0.25f) hpBarFG_->SetColor(VectorColorCodes::Red);
		else if (ratio < 0.5f) hpBarFG_->SetColor(VectorColorCodes::Yellow);
		else hpBarFG_->SetColor(VectorColorCodes::Lime);
		
		hpBarFG_->Update();
		hpBarFrame_->Update();
	}
}

void ControlsGuide::Draw()
{
	if (!isVisible_) return;

	for (auto& prompt : prompts_)
	{
		prompt.icon_->Draw();
		prompt.overlay_->Draw();
		prompt.description_->Draw();
	}

	hpBarFG_->Draw();
	hpBarFrame_->Draw();
}

void ControlsGuide::SetVisible(bool isVisible)
{
	isVisible_ = isVisible;
}

bool ControlsGuide::IsVisible() const
{
	return isVisible_;
}

void ControlsGuide::AddPrompt(const std::string& actionName, const std::string& iconPath, const std::string& descriptionPath)
{
	ControlPrompt prompt;
	prompt.actionName_ = actionName;
	prompt.iconPath_ = iconPath;

	float y = kPromptBaseY + (prompts_.size() * kPromptSpacingY);
	prompt.y_ = y;

	// アイコン (ベース)
	prompt.icon_ = std::make_unique<Sprite>();
	prompt.icon_->Initialize(spriteCommon_, iconPath);
	prompt.icon_->SetPosition({ kPromptBaseX, y });
	prompt.icon_->SetSize({ 75.0f, 75.0f });
	prompt.icon_->SetAnchorPoint({ 0.5f, 0.5f });
	prompt.icon_->SetColor({ 0.3f, 0.3f, 0.3f, 1.0f }); // デフォルトを少し暗めに

	// オーバーレイ（同じテクスチャで明るい色）
	prompt.overlay_ = std::make_unique<Sprite>();
	prompt.overlay_->Initialize(spriteCommon_, iconPath);
	prompt.overlay_->SetPosition({ kPromptBaseX, y });
	prompt.overlay_->SetSize({ 75.0f, 75.0f });
	prompt.overlay_->SetColor({ 1, 1, 1, 0 }); // 初期は透明
	prompt.overlay_->SetAnchorPoint({ 0.5f, 0.5f });

	// 説明テキスト
	prompt.description_ = std::make_unique<Sprite>();
	prompt.description_->Initialize(spriteCommon_, descriptionPath);
	prompt.description_->SetPosition({ kPromptBaseX + 50.0f, y });
	prompt.description_->SetSize({ 138.0f, 48.0f }); 
	prompt.description_->SetAnchorPoint({ 0.0f, 0.5f });

	prompts_.push_back(std::move(prompt));
}
