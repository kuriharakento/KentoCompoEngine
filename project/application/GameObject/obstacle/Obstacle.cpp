#include "Obstacle.h"

#include "application/GameObject/character/base/Character.h"
#include "application/GameObject/component/collision/CollisionUtils.h"
#include "application/GameObject/component/collision/OBBColliderComponent.h"
#include "base/Logger.h"
#include "math/OBB.h"

void Obstacle::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager)
{
	// ゲームオブジェクトの初期化
	GameObject::Initialize(object3dCommon, lightManager);

	// OBBコライダーコンポーネントを追加
	AddComponent("OBBColliderComponent", std::make_unique<OBBColliderComponent>(this));
}

void Obstacle::Update()
{
	// ゲームオブジェクトの更新
	GameObject::Update();
}

void Obstacle::Draw(CameraManager* camera)
{
	// ゲームオブジェクトの描画処理
	GameObject::Draw(camera);
}

void Obstacle::AddComponent(const std::string& name, std::unique_ptr<IGameObjectComponent> comp)
{
	if (auto collider = dynamic_cast<ICollisionComponent*>(comp.get()))
	{
		// 衝突判定コンポーネントの場合は、衝突時の処理を設定
		CollisionSettings(collider);
	}
	// コンポーネントを追加
	GameObject::AddComponent(name, std::move(comp));
}

void Obstacle::CollisionSettings(ICollisionComponent* collider)
{
	auto onResolve = [this](GameObject* other) {
		ResolvePenetration(other);
		};
	collider->SetOnEnter(onResolve);
	collider->SetOnStay(onResolve);
	collider->SetOnExit([this](GameObject* other) {
		// 衝突が離れた時の処理
		auto character = dynamic_cast<Character*>(other);
		if (character)
		{
			if (character->IsGrounded())
			{
				// 接地状態を解除
				character->SetIsGrounded(false);
			}
		}
						});
}

bool Obstacle::CheckOBBvsOBBMTV(const OBB& obbA, const OBB& obbB, Vector3& mtv) const
{
	// 各 OBB のワールド軸ベクトルを取得
	Matrix4x4 rotA = obbA.rotate;
	Matrix4x4 rotB = obbB.rotate;
	Vector3 axesA[3] = {
		Vector3::Normalize({rotA.m[0][0], rotA.m[0][1], rotA.m[0][2]}),
		Vector3::Normalize({rotA.m[1][0], rotA.m[1][1], rotA.m[1][2]}),
		Vector3::Normalize({rotA.m[2][0], rotA.m[2][1], rotA.m[2][2]})
	};
	Vector3 axesB[3] = {
		Vector3::Normalize({rotB.m[0][0], rotB.m[0][1], rotB.m[0][2]}),
		Vector3::Normalize({rotB.m[1][0], rotB.m[1][1], rotB.m[1][2]}),
		Vector3::Normalize({rotB.m[2][0], rotB.m[2][1], rotB.m[2][2]})
	};

	// 分離軸リスト（Aの軸、Bの軸、クロス積軸）
	std::vector<Vector3> testAxes;
	testAxes.reserve(15);
	for (int i = 0; i < 3; ++i) testAxes.push_back(axesA[i]);
	for (int i = 0; i < 3; ++i) testAxes.push_back(axesB[i]);
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			testAxes.push_back(Vector3::Normalize(Vector3::Cross(axesA[i], axesB[j])));

	Vector3 toCenter = obbB.center - obbA.center;
	CollisionInfo info;

	for (auto& axis : testAxes)
	{
		if (axis.LengthSquared() < 1e-6f) continue;

		float projA = std::abs(Vector3::Dot(axesA[0] * obbA.size.x, axis))
			+ std::abs(Vector3::Dot(axesA[1] * obbA.size.y, axis))
			+ std::abs(Vector3::Dot(axesA[2] * obbA.size.z, axis));
		float projB = std::abs(Vector3::Dot(axesB[0] * obbB.size.x, axis))
			+ std::abs(Vector3::Dot(axesB[1] * obbB.size.y, axis))
			+ std::abs(Vector3::Dot(axesB[2] * obbB.size.z, axis));
		float dist = std::abs(Vector3::Dot(toCenter, axis));
		float overlap = (projA + projB) - dist;

		if (overlap < 0)
		{
			return false;
		}
		if (overlap < info.mtvDepth)
		{
			info.isColliding = true;
			info.mtvDepth = overlap;
			info.mtvAxis = axis;
		}
	}

	// 衝突時に MTV を算出
	if (info.isColliding)
	{
		if (Vector3::Dot(info.mtvAxis, toCenter) < 0.0f)
			info.mtvAxis = info.mtvAxis * -1.0f;
		mtv = info.mtvAxis * info.mtvDepth;
	}
	return info.isColliding;
}

void Obstacle::ResolvePenetration(GameObject* other)
{
	auto character = dynamic_cast<Character*>(other);
	if (!character) return; // 他のキャラクターでない場合は何もしない

	auto obstacleColl = GetComponent<OBBColliderComponent>();
	auto otherColl = other->GetComponent<OBBColliderComponent>();
	if (!obstacleColl || !otherColl) return;
	// 衝突した位置を取得
	Vector3 collisionPos = otherColl->GetCollisionPosition();

	// MTV計算用にOBBのcenterを衝突位置に変更
	OBB obbA = obstacleColl->GetOBB();
	OBB obbB = otherColl->GetOBB();
	obbB.center = collisionPos;

	Vector3 mtv;
	if (CheckOBBvsOBBMTV(obbA, obbB, mtv))
	{
		other->SetPosition(obbB.center + mtv);
		// MTVのY成分が十分大きい場合のみ接地判定
		if (mtv.y > 0.1f)
		{
			character->SetIsGrounded(true);
		}
	}
}