#include "SpriteRendererComponent.h"

#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/component/base/ComponentFactory.h"
#include "engine/gameobject/component/render/SpriteRenderUtil.h"
#include "engine/gameobject/manager/GameObjectManager.h"

namespace KCE
{
REGISTER_RENDERER_COMPONENT(SpriteRendererComponent)

namespace GameObjectComponent
{
void ForceLinkSpriteRendererComponent() {}

SpriteRendererComponent::SpriteRendererComponent()
{
	Register("texture", &texture_);
	Register("width", &width_);
	Register("height", &height_);
	Register("anchorX", &anchorX_);
	Register("anchorY", &anchorY_);
	Register("color", &color_);
	Register("flipX", &flipX_);
	Register("flipY", &flipY_);
	Register("order", &order_);
}

void SpriteRendererComponent::Awake()
{
	if (GameObject* owner = GetOwner())
	{
		// GameObject は作られるときに立方体を持つ。2D には要らないので映さない
		owner->SetBodyVisible(false);
	}
}

bool SpriteRendererComponent::EnsureSprite()
{
	if (sprite_)
	{
		return true;
	}
	GameObjectManager* manager = GameObjectManager::HasInstance() ? GameObjectManager::GetInstance() : nullptr;
	SpriteCommon* common = manager ? manager->GetSpriteCommon() : nullptr;
	if (!common)
	{
		return false;
	}
	// 未読み込みの名前なら、必ずある白で始める（あとで読み込まれたら ApplySettings が差し替える）
	const std::string initial = IsTextureLoaded(texture_) ? texture_ : std::string(kDefaultSpriteTexture);
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(common, initial);
	appliedTexture_ = initial;
	if (width_ <= 0.0f || height_ <= 0.0f)
	{
		// 画像の大きさで始める
		width_ = sprite_->GetSize().x;
		height_ = sprite_->GetSize().y;
	}
	return true;
}

void SpriteRendererComponent::ApplySettings()
{
	// プレハブ読み込みやインスペクタで変わった画像を拾う。読み込み済みのものだけ
	if (appliedTexture_ != texture_ && IsTextureLoaded(texture_))
	{
		sprite_->SetTexture(texture_);
		appliedTexture_ = texture_;
	}
	sprite_->SetColor(color_);
	sprite_->SetAnchorPoint({ anchorX_, anchorY_ });
	sprite_->SetFlipX(flipX_);
	sprite_->SetFlipY(flipY_);
}

void SpriteRendererComponent::DrawSprite2D()
{
	if (!IsEnabled() || !EnsureSprite())
	{
		return;
	}
	GameObject* owner = GetOwner();
	if (!owner)
	{
		return;
	}
	ApplySettings();

	const ScreenPlacement placement = DecomposeScreenPlacement(owner->GetRenderWorldMatrix());
	sprite_->SetPosition({ placement.x, placement.y });
	sprite_->SetSize({ width_ * placement.scaleX, height_ * placement.scaleY });
	sprite_->SetRotation(placement.rotation);
	sprite_->Update();
	sprite_->Draw();
}

#ifdef USE_IMGUI
void SpriteRendererComponent::DrawImGui()
{
	// 数値の項目（サイズ・アンカー・反転・order など）は、登録した値から出る
	JsonEditableBase::DrawImGui();

	DrawTextureCombo("画像", texture_);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("読み込み済みの画像から選ぶ。Project ウィンドウで読み込める");
	}
	ImGui::ColorEdit4("色", &color_.x);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("位置は Transform の XY（画面の px）、回転は Z、描く順は order（小さいほど奥）");
	}
}
#endif
} // namespace GameObjectComponent
} // namespace KCE
