#include "RayColliderComponent.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/manager/graphics/LineManager.h"
#include "math/MathUtils.h"

RayColliderComponent::RayColliderComponent(GameObject* owner)
	: ICollisionComponent(owner)
{
}

void RayColliderComponent::Init()
{
	// 初期状態のレイ情報を構築
	Update(owner_);
}

void RayColliderComponent::Update(GameObject* owner)
{
	if (!owner_) return;

	Vector3 worldPos = owner_->GetPosition() + offset_;
	ray_.start = worldPos;

	if (useWorldDirection_)
	{
		// ワールド空間で直接指定された方向をそのまま使う（所有者の回転は適用しない）
		ray_.direction = worldDirection_;
	}
	else
	{
		// baseDirection_ を所有者の回転で変換してワールド方向を算出
		Vector3 rotation = owner_->GetRotation();
		Matrix4x4 rotMatrix = MakeRotateMatrix(rotation);
		Vector3 worldDir = MathUtils::TransformNormal(baseDirection_, rotMatrix);
		worldDir.NormalizeSelf();
		ray_.direction = worldDir;
	}
}

void RayColliderComponent::Draw()
{
#ifdef USE_IMGUI
	// 衝突判定が無効な場合は描画しない
	if (!owner_ || !owner_->IsActive()) return;

	// レイの終点を計算
	Vector3 endPoint = ray_.start + ray_.direction * ray_.length;

	// 線として描画（緑色）
	Vector4 color = { 0.0f, 1.0f, 0.0f, 1.0f };
	LineManager::GetInstance()->DrawLine(ray_.start, endPoint, color);
#endif
}
