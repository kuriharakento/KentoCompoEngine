#include "RayCollider.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/manager/graphics/LineManager.h"
#include "math/MathUtils.h"

// Factory
#include "engine/gameobject/component/base/ComponentFactory.h"

namespace KCE
{
REGISTER_COMPONENT(RayCollider)
REGISTER_COMPONENT_ALIAS(RayCollider, RayColliderComponent)

namespace GameObjectComponent
{
	RayCollider::RayCollider()
	{
		Register("offset", &offset_);
		Register("baseDirection", &baseDirection_);
		Register("length", &ray_.length);
		Register("useWorldDirection", &useWorldDirection_);
		Register("worldDirection", &worldDirection_);
		// プレハブで作り直したときに、当たる相手の設定が既定値（None）に戻らないよう保存する
		Register("collisionLayer", &layer_);
		Register("collisionMask", &collisionMask_);
	}

	void RayCollider::Awake()
	{
		// 初期状態のレイ情報を構築
		Update();
	}

	void RayCollider::Update()
	{
		if (!GetOwner()) return;

		const Matrix4x4 world = GetOwner()->GetWorldMatrix();
		ray_.start = MathUtils::Transform(offset_, world);

		if (useWorldDirection_)
		{
			// ワールド空間で直接指定された方向をそのまま使う（所有者の回転は適用しない）
			ray_.direction = worldDirection_;
		}
		else
		{
			// baseDirection_ を所有者の回転で変換してワールド方向を算出
			Vector3 worldDir = MathUtils::TransformNormal(baseDirection_, world);
			worldDir.NormalizeSelf();
			ray_.direction = worldDir;
		}
		Draw();
	}

	void RayCollider::SetWorldDirection(const Vector3& dir)
	{
		constexpr float kMinimumDirectionLength = 0.000001f;
		worldDirection_ = dir;
		if (worldDirection_.Length() <= kMinimumDirectionLength)
		{
			worldDirection_ = { 0.0f, 0.0f, 1.0f };
		}
		else
		{
			worldDirection_.NormalizeSelf();
		}
		useWorldDirection_ = true;
	}

	void RayCollider::Draw()
	{
	#ifdef USE_IMGUI
		// 衝突判定が無効な場合は描画しない
		if (!GetOwner() || !GetOwner()->IsActive()) return;

		// レイの終点を計算
		Vector3 endPoint = ray_.start + ray_.direction * ray_.length;

		// 線として描画（緑色）
		Vector4 color = { 0.0f, 1.0f, 0.0f, 1.0f };
		LineManager::GetInstance()->DrawLine(ray_.start, endPoint, color);
	#endif
	}
}
} // namespace KCE
