#include "GaugeComponent.h"

#include <algorithm>
#include <cmath>

#include "base/Camera.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/component/base/ComponentFactory.h"
#include "engine/gameobject/component/render/SpriteRenderUtil.h"
#include "engine/gameobject/manager/GameObjectManager.h"
#include "graphics/2d/SpriteCommon.h"
#include "graphics/3d/Object3dCommon.h"
#include "graphics/FrameConstantAllocator.h"
#include "manager/scene/CameraManager.h"

namespace KCE
{
REGISTER_RENDERER_COMPONENT(GaugeComponent)

namespace GameObjectComponent
{
void ForceLinkGaugeComponent() {}

namespace
{
constexpr float kDegToRad = 0.01745329252f;
} // namespace

GaugeComponent::GaugeComponent()
{
	Register("foreTexture", &foreTexture_);
	Register("backTexture", &backTexture_);
	Register("width", &width_);
	Register("height", &height_);
	Register("anchorX", &anchorX_);
	Register("anchorY", &anchorY_);
	Register("ratio", &ratio_);
	Register("followSpeed", &followSpeed_);
	Register("startAngleDeg", &startAngleDeg_);
	Register("clockwise", &clockwise_);
	Register("tint", &tint_);
	Register("backTint", &backTint_);
	Register("colorStops", &colorStops_);
	Register("order", &order_);
	Register("world3d", &world3d_);
	Register("billboard", &billboard_);
	Register("depthTest", &depthTest_);
	Register("pixelsPerUnit", &pixelsPerUnit_);
}

void GaugeComponent::Awake()
{
	if (GameObject* owner = GetOwner())
	{
		// GameObject は作られるときに立方体を持つ。ゲージには要らないので映さない
		owner->SetBodyVisible(false);
	}
}

void GaugeComponent::Update()
{
	if (!displayInitialized_)
	{
		displayRatio_ = std::clamp(ratio_, 0.0f, 1.0f);
		displayInitialized_ = true;
		return;
	}
	const float target = std::clamp(ratio_, 0.0f, 1.0f);
	if (followSpeed_ <= 0.0f)
	{
		displayRatio_ = target;
		return;
	}
	// 目標との差を、時間に応じた割合で縮める（フレームの長さによらず同じ追いかけ方になる）
	const float rate = 1.0f - std::exp(-followSpeed_ * GetDeltaTime());
	displayRatio_ += (target - displayRatio_) * rate;
}

void GaugeComponent::SetRatio(float ratio)
{
	ratio_ = std::clamp(ratio, 0.0f, 1.0f);
}

void GaugeComponent::SetRatioImmediate(float ratio)
{
	ratio_ = std::clamp(ratio, 0.0f, 1.0f);
	displayRatio_ = ratio_;
	displayInitialized_ = true;
}

void GaugeComponent::SetTextures(const std::string& foreTexture, const std::string& backTexture)
{
	foreTexture_ = foreTexture;
	backTexture_ = backTexture;
}

void GaugeComponent::SetColorStops(const std::vector<ColorStop>& stops)
{
	colorStops_.clear();
	for (const ColorStop& stop : stops)
	{
		colorStops_.push_back({ stop.color.x, stop.color.y, stop.color.z, stop.ratio });
	}
}

Vector4 GaugeComponent::EvaluateColor(float ratio) const
{
	if (colorStops_.empty())
	{
		return tint_;
	}
	// 割合以下で一番大きい段階（lo）と、割合以上で一番小さい段階（hi）を探す
	const Vector4* lo = nullptr;
	const Vector4* hi = nullptr;
	for (const Vector4& stop : colorStops_)
	{
		if (stop.w <= ratio && (!lo || stop.w > lo->w)) { lo = &stop; }
		if (stop.w >= ratio && (!hi || stop.w < hi->w)) { hi = &stop; }
	}
	// 端の外は、いちばん近い段階の色のまま
	if (!lo) { lo = hi; }
	if (!hi) { hi = lo; }
	float t = 0.0f;
	if (hi->w > lo->w)
	{
		t = (ratio - lo->w) / (hi->w - lo->w);
	}
	const float r = lo->x + (hi->x - lo->x) * t;
	const float g = lo->y + (hi->y - lo->y) * t;
	const float b = lo->z + (hi->z - lo->z) * t;
	return { r * tint_.x, g * tint_.y, b * tint_.z, tint_.w };
}

void GaugeComponent::SyncDisplayRatio()
{
	// 更新が止まっている間（エディタの編集中）は Update が来ないので、目標へそのまま合わせる
	GameObject* owner = GetOwner();
	if (!displayInitialized_ || (owner && !owner->IsUpdateEnabled()))
	{
		displayRatio_ = std::clamp(ratio_, 0.0f, 1.0f);
		displayInitialized_ = true;
	}
}

bool GaugeComponent::Prepare()
{
	GameObjectManager* manager = GameObjectManager::HasInstance() ? GameObjectManager::GetInstance() : nullptr;
	SpriteCommon* common = manager ? manager->GetSpriteCommon() : nullptr;
	if (!common)
	{
		return false;
	}

	// 前景。未読み込みの名前なら、必ずある白で始める
	if (!foreSprite_)
	{
		const std::string initial = IsTextureLoaded(foreTexture_) ? foreTexture_ : std::string(kDefaultSpriteTexture);
		foreSprite_ = std::make_unique<Sprite>();
		foreSprite_->Initialize(common, initial);
		appliedForeTexture_ = initial;
	}
	if (appliedForeTexture_ != foreTexture_ && IsTextureLoaded(foreTexture_))
	{
		foreSprite_->SetTexture(foreTexture_);
		appliedForeTexture_ = foreTexture_;
	}

	// 背景。空なら描かない
	if (!backTexture_.empty() && IsTextureLoaded(backTexture_))
	{
		if (!backSprite_)
		{
			backSprite_ = std::make_unique<Sprite>();
			backSprite_->Initialize(common, backTexture_);
			appliedBackTexture_ = backTexture_;
		}
		else if (appliedBackTexture_ != backTexture_)
		{
			backSprite_->SetTexture(backTexture_);
			appliedBackTexture_ = backTexture_;
		}
	}

	SyncDisplayRatio();
	foreSprite_->SetAnchorPoint({ anchorX_, anchorY_ });
	foreSprite_->SetColor(EvaluateColor(displayRatio_));
	foreSprite_->SetRadialFill(displayRatio_, startAngleDeg_ * kDegToRad, clockwise_);
	if (backSprite_)
	{
		backSprite_->SetAnchorPoint({ anchorX_, anchorY_ });
		backSprite_->SetColor(backTint_);
		backSprite_->SetRadialFill(1.0f);
	}
	return true;
}

void GaugeComponent::DrawSprite2D()
{
	GameObject* owner = GetOwner();
	if (!IsEnabled() || world3d_ || !owner || !Prepare())
	{
		return;
	}
	const ScreenPlacement placement = DecomposeScreenPlacement(owner->GetRenderWorldMatrix());
	const Vector2 size = { width_ * placement.scaleX, height_ * placement.scaleY };
	const bool drawBack = backSprite_ && !backTexture_.empty();

	if (drawBack)
	{
		backSprite_->SetPosition({ placement.x, placement.y });
		backSprite_->SetSize(size);
		backSprite_->SetRotation(placement.rotation);
		backSprite_->Update();
		backSprite_->Draw();
	}
	foreSprite_->SetPosition({ placement.x, placement.y });
	foreSprite_->SetSize(size);
	foreSprite_->SetRotation(placement.rotation);
	foreSprite_->Update();
	foreSprite_->Draw();
}

void GaugeComponent::Draw3D(CameraManager* cameraManager)
{
	GameObject* owner = GetOwner();
	if (!IsEnabled() || !world3d_ || !owner || !cameraManager)
	{
		return;
	}
	Camera* camera = cameraManager->GetActiveCamera();
	if (!camera || !Prepare())
	{
		return;
	}

	// 持ち主のワールド行列（行ベクトル）。位置は m[3]、各軸は m[0]〜m[2]
	const Matrix4x4& ownerWorld = owner->GetRenderWorldMatrix();
	// 単位の四角（アンカー引き済み、Y が下向き）を、ワールドの大きさへ。上を向けるので Y を反転する
	const float unitsPerPixel = pixelsPerUnit_ > 0.0f ? 1.0f / pixelsPerUnit_ : 0.01f;
	const float localScaleX = width_ * unitsPerPixel;
	const float localScaleY = -height_ * unitsPerPixel;

	Matrix4x4 world = ownerWorld;
	if (billboard_)
	{
		// カメラの向きの軸（右・上・前）。ビュー行列の左上 3x3 の転置にあたる
		const Matrix4x4& view = camera->GetViewMatrix();
		const float scaleX = std::sqrt(ownerWorld.m[0][0] * ownerWorld.m[0][0] + ownerWorld.m[0][1] * ownerWorld.m[0][1] + ownerWorld.m[0][2] * ownerWorld.m[0][2]);
		const float scaleY = std::sqrt(ownerWorld.m[1][0] * ownerWorld.m[1][0] + ownerWorld.m[1][1] * ownerWorld.m[1][1] + ownerWorld.m[1][2] * ownerWorld.m[1][2]);
		for (int axis = 0; axis < 3; ++axis)
		{
			world.m[axis][0] = view.m[0][axis];
			world.m[axis][1] = view.m[1][axis];
			world.m[axis][2] = view.m[2][axis];
			world.m[axis][3] = 0.0f;
		}
		for (int column = 0; column < 3; ++column)
		{
			world.m[0][column] *= localScaleX * scaleX;
			world.m[1][column] *= localScaleY * scaleY;
		}
		world.m[3][3] = 1.0f;
	}
	else
	{
		// 持ち主の向きのまま。各軸を大きさぶん伸ばす（行列の左から拡大を掛けるのと同じ）
		for (int column = 0; column < 3; ++column)
		{
			world.m[0][column] *= localScaleX;
			world.m[1][column] *= localScaleY;
		}
	}

	SpriteCommon* common = GameObjectManager::GetInstance()->GetSpriteCommon();
	if (!common)
	{
		return;
	}
	common->SceneRenderingSetting(depthTest_);
	const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();
	// 1フレームに複数のビューから呼ばれるので、行列は描くたびに別の領域へ置く
	Object3dCommon* object3dCommon = owner->GetObject3dCommon();
	FrameConstantAllocator* allocator = object3dCommon ? object3dCommon->GetFrameConstantAllocator() : nullptr;
	if (!allocator)
	{
		return;
	}
	if (backSprite_ && !backTexture_.empty())
	{
		backSprite_->UpdateWithMatrix(world, viewProjection);
		backSprite_->DrawWithMatrix(world, viewProjection, allocator);
	}
	foreSprite_->UpdateWithMatrix(world, viewProjection);
	foreSprite_->DrawWithMatrix(world, viewProjection, allocator);
}

#ifdef USE_IMGUI
void GaugeComponent::DrawImGui()
{
	// 数値・チェックの項目は、登録した値から出る
	JsonEditableBase::DrawImGui();

	// 反映は Prepare が、読み込み済みのときだけ行う
	DrawTextureCombo("前景の画像", foreTexture_);
	DrawTextureCombo("背景の画像", backTexture_, true);
	ImGui::ColorEdit4("前景の色", &tint_.x);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("前景の基準の色。色の段階の色に掛ける。段階が無いときは、この色のまま");
	}
	ImGui::ColorEdit4("背景の色", &backTint_.x);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("背景の色。前景とは別に決められる");
	}
	ImGui::SliderFloat("割合", &ratio_, 0.0f, 1.0f);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("目標の割合。見た目は followSpeed でなめらかに追いかける");
	}

	// 割合 → 色の段階
	ImGui::SeparatorText("色の段階");
	int removeIndex = -1;
	for (size_t index = 0; index < colorStops_.size(); ++index)
	{
		ImGui::PushID(static_cast<int>(index));
		Vector4& stop = colorStops_[index];
		ImGui::SetNextItemWidth(100.0f);
		ImGui::SliderFloat("##ratio", &stop.w, 0.0f, 1.0f, "割合 %.2f");
		ImGui::SameLine();
		ImGui::ColorEdit3("##color", &stop.x, ImGuiColorEditFlags_NoInputs);
		ImGui::SameLine();
		if (ImGui::SmallButton("削除"))
		{
			removeIndex = static_cast<int>(index);
		}
		ImGui::PopID();
	}
	if (removeIndex >= 0)
	{
		colorStops_.erase(colorStops_.begin() + removeIndex);
	}
	if (ImGui::Button("段階を追加"))
	{
		colorStops_.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("例: 1.0 を緑、0.5 を黄、0.2 を赤にすると、減るにつれて色が変わる");
	}
}
#endif
} // namespace GameObjectComponent
} // namespace KCE
