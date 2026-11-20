#include "SpriteSheetComponent.h"

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
REGISTER_RENDERER_COMPONENT(SpriteSheetComponent)

namespace GameObjectComponent
{
void ForceLinkSpriteSheetComponent() {}

SpriteSheetComponent::SpriteSheetComponent()
{
	Register("texture", &texture_);
	Register("columns", &columns_);
	Register("rows", &rows_);
	Register("frame", &frame_);
	Register("width", &width_);
	Register("height", &height_);
	Register("anchorX", &anchorX_);
	Register("anchorY", &anchorY_);
	Register("tint", &tint_);
	Register("order", &order_);
	Register("world3d", &world3d_);
	Register("billboard", &billboard_);
	Register("depthTest", &depthTest_);
	Register("pixelsPerUnit", &pixelsPerUnit_);
}

void SpriteSheetComponent::Awake()
{
	if (GameObject* owner = GetOwner())
	{
		// GameObject は作られるときに立方体を持つ。板には要らないので映さない
		owner->SetBodyVisible(false);
	}
}

void SpriteSheetComponent::SetSheet(const std::string& texture, int columns, int rows)
{
	texture_ = texture;
	columns_ = columns;
	rows_ = rows;
}

bool SpriteSheetComponent::Prepare()
{
	GameObjectManager* manager = GameObjectManager::HasInstance() ? GameObjectManager::GetInstance() : nullptr;
	SpriteCommon* common = manager ? manager->GetSpriteCommon() : nullptr;
	if (!common)
	{
		return false;
	}

	// 画像。未読み込みの名前なら、必ずある白で始める
	if (!sprite_)
	{
		const std::string initial = IsTextureLoaded(texture_) ? texture_ : std::string(kDefaultSpriteTexture);
		sprite_ = std::make_unique<Sprite>();
		sprite_->Initialize(common, initial);
		appliedTexture_ = initial;
		// 初期化の直後は、切り出しが画像全体になっている
		sheetSize_ = sprite_->GetTextureSize();
	}
	if (appliedTexture_ != texture_ && IsTextureLoaded(texture_))
	{
		sprite_->SetTexture(texture_);
		appliedTexture_ = texture_;
		sheetSize_ = sprite_->GetTextureSize();
	}

	// 映すコマ。番号は範囲内に丸める
	const int columns = (std::max)(columns_, 1);
	const int rows = (std::max)(rows_, 1);
	const int frame = std::clamp(frame_, 0, columns * rows - 1);
	const Vector2 cell = { sheetSize_.x / static_cast<float>(columns), sheetSize_.y / static_cast<float>(rows) };
	sprite_->SetTextureLeftTop({ cell.x * static_cast<float>(frame % columns), cell.y * static_cast<float>(frame / columns) });
	sprite_->SetTextureSize(cell);

	sprite_->SetAnchorPoint({ anchorX_, anchorY_ });
	sprite_->SetColor(tint_);
	return true;
}

void SpriteSheetComponent::DrawSprite2D()
{
	GameObject* owner = GetOwner();
	if (!IsEnabled() || world3d_ || !owner || !Prepare())
	{
		return;
	}
	const ScreenPlacement placement = DecomposeScreenPlacement(owner->GetRenderWorldMatrix());
	sprite_->SetPosition({ placement.x, placement.y });
	sprite_->SetSize({ width_ * placement.scaleX, height_ * placement.scaleY });
	sprite_->SetRotation(placement.rotation);
	sprite_->Update();
	sprite_->Draw();
}

void SpriteSheetComponent::DrawTransparent3D(CameraManager* cameraManager)
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
		// 持ち主の向きのまま。各軸を大きさぶん伸ばす
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
	sprite_->UpdateWithMatrix(world, viewProjection);
	sprite_->DrawWithMatrix(world, viewProjection, allocator);
}

#ifdef USE_IMGUI
void SpriteSheetComponent::DrawImGui()
{
	// 数値・チェックの項目は、登録した値から出る
	JsonEditableBase::DrawImGui();

	// 画像は、読み込み済みの物から選ばせる。反映は Prepare が行う
	DrawTextureCombo("画像", texture_);
	ImGui::ColorEdit4("色", &tint_.x);
	ImGui::SliderInt("コマ", &frame_, 0, GetFrameCount() - 1);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("映すコマの番号。0 は左上。右へ進み、行が終わったら次の行");
	}
}
#endif
} // namespace GameObjectComponent
} // namespace KCE
