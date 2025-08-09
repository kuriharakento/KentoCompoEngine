#include "CollisionManager.h"
#include <algorithm>

#include "math/AABB.h"
#include "application/GameObject/component/collision/AABBColliderComponent.h"
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "application/GameObject/base/GameObject.h"
#include "base/Logger.h"
#include "imgui/imgui.h"
#include "math/MathUtils.h"

CollisionManager* CollisionManager::instance_ = nullptr; // シングルトンインスタンス

CollisionManager* CollisionManager::GetInstance()
{
	if (instance_ == nullptr)
	{
		instance_ = new CollisionManager();
	}
	return instance_;
}

void CollisionManager::Register(ICollisionComponent* collider)
{
	colliders_.push_back(collider);
}

void CollisionManager::Unregister(ICollisionComponent* collider)
{
	// currentCollisions_ から該当コライダーを含むペアを削除
	for (auto it = currentCollisions_.begin(); it != currentCollisions_.end(); )
	{
		if (it->a == collider || it->b == collider)
		{
			it = currentCollisions_.erase(it);
		}
		else
		{
			++it;
		}
	}

	colliders_.erase(std::remove(colliders_.begin(), colliders_.end(), collider), colliders_.end());
}

void CollisionManager::CheckCollisions()
{
#ifdef _DEBUG
	ImGui::Begin("CollisionManager Colliders");

	ImGui::SeparatorText("Colliders");
	if (ImGui::CollapsingHeader("List"))
	{
		for (size_t i = 0; i < colliders_.size(); ++i)
		{
			ICollisionComponent* collider = colliders_[i];
			if (collider && collider->GetOwner())
			{
				ImGui::Text("Collider %zu: %s", i, collider->GetOwner()->GetTag().c_str());
				ImGui::Text("Position: (%.2f, %.2f, %.2f)", collider->GetOwner()->GetPosition().x, collider->GetOwner()->GetPosition().y, collider->GetOwner()->GetPosition().z);
				ImGui::Text("Previous Position: (%.2f, %.2f, %.2f)", collider->GetPreviousPosition().x, collider->GetPreviousPosition().y, collider->GetPreviousPosition().z);
			}
			else
			{
				ImGui::Text("Collider %zu: nullptr", i);
			}
			ImGui::Separator();
		}
	}

	ImGui::SeparatorText("Current Collisions");

	ImGui::Text("Registered Colliders: %zu", colliders_.size());
	for (size_t i = 0; i < colliders_.size(); ++i)
	{
		ICollisionComponent* collider = colliders_[i];
		std::string label = std::to_string(i) + ": ";
		if (collider && collider->GetOwner())
		{
			label += collider->GetOwner()->GetTag();
			label += " (" + GetColliderTypeString(collider->GetColliderType()) + ")";
			label += ",  substep: " + std::to_string(collider->UseSubstep());
		}
		else
		{
			label += "nullptr";
		}
		ImGui::Text("%s", label.c_str());
	}

	

	ImGui::End();
#endif

	std::unordered_set<CollisionPair, CollisionPairHash> newCollisions;

	for (size_t i = 0; i < colliders_.size(); ++i)
	{
		for (size_t j = i + 1; j < colliders_.size(); ++j)
		{
			ICollisionComponent* a = colliders_[i];
			ICollisionComponent* b = colliders_[j];

			bool isHit = false;

			// 衝突判定のディスパッチ
			ColliderType typeA = a->GetColliderType();
			ColliderType typeB = b->GetColliderType();

			//コライダーのタイプごとに衝突判定を行う
			/* AABB vs AABB */
			if (typeA == ColliderType::AABB && typeB == ColliderType::AABB)
			{
				if (a->UseSubstep() || b->UseSubstep())
				{
					isHit = CheckSubstepCollision(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
				}
				else
				{
					isHit = CheckCollision(static_cast<AABBColliderComponent*>(a), static_cast<AABBColliderComponent*>(b));
				}
			}
			/* OBB vs OBB */
			else if (typeA == ColliderType::OBB && typeB == ColliderType::OBB)
			{
				if (a->UseSubstep() || b->UseSubstep())
				{
					isHit = CheckSubstepCollision(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				else
				{
					isHit = CheckCollision(static_cast<OBBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
			}
			/* AABB vs OBB */
			else if (typeA == ColliderType::AABB && typeB == ColliderType::OBB)
			{
				if (a->UseSubstep() || b->UseSubstep())
				{
					isHit = CheckSubstepCollision(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
				else
				{
					isHit = CheckCollision(static_cast<AABBColliderComponent*>(a), static_cast<OBBColliderComponent*>(b));
				}
			}
			else if (typeA == ColliderType::OBB && typeB == ColliderType::AABB)
			{
				if (a->UseSubstep() || b->UseSubstep())
				{
					isHit = CheckSubstepCollision(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
				}
				else
				{
					isHit = CheckCollision(static_cast<AABBColliderComponent*>(b), static_cast<OBBColliderComponent*>(a));
				}
			}

			// 衝突している場合
			if (isHit)
			{
				CollisionPair pair{ a, b };
				newCollisions.insert(pair);
				//衝突した瞬間の処理
				if (!currentCollisions_.contains(pair))
				{
					a->CallOnEnter(b->GetOwner());
					b->CallOnEnter(a->GetOwner());
				}
				else
				{
					//衝突している間の処理
					a->CallOnStay(b->GetOwner());
					b->CallOnStay(a->GetOwner());
				}
			}
		}
	}

	// 離れた衝突を処理
	for (const auto& pair : currentCollisions_)
	{
		if (!newCollisions.contains(pair))
		{
			//衝突が離れた場合の処理
			pair.a->CallOnExit(pair.b->GetOwner());
			pair.b->CallOnExit(pair.a->GetOwner());
		}
	}

	currentCollisions_ = std::move(newCollisions);
}

void CollisionManager::UpdatePreviousPositions()
{
	for (auto& collider : colliders_)
	{
		collider->SetPreviousPosition(collider->GetOwner()->GetPosition());
	}
}

bool CollisionManager::CheckCollision(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	return (aBox.max_.x >= bBox.min_.x && aBox.min_.x <= bBox.max_.x) &&
		(aBox.max_.y >= bBox.min_.y && aBox.min_.y <= bBox.max_.y) &&
		(aBox.max_.z >= bBox.min_.z && aBox.min_.z <= bBox.max_.z);
}

bool CollisionManager::CheckCollision(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	const OBB& obbA = a->GetOBB();
	const OBB& obbB = b->GetOBB();

	// OBBの回転行列（各軸ベクトル）
	Matrix4x4 rotA = obbA.rotate;
	Matrix4x4 rotB = obbB.rotate;

	Vector3 axesA[3] =
	{
		Vector3::Normalize(Vector3(rotA.m[0][0], rotA.m[0][1], rotA.m[0][2])),
		Vector3::Normalize(Vector3(rotA.m[1][0], rotA.m[1][1], rotA.m[1][2])),
		Vector3::Normalize(Vector3(rotA.m[2][0], rotA.m[2][1], rotA.m[2][2]))
	};

	Vector3 axesB[3] =
	{
		Vector3::Normalize(Vector3(rotB.m[0][0], rotB.m[0][1], rotB.m[0][2])),
		Vector3::Normalize(Vector3(rotB.m[1][0], rotB.m[1][1], rotB.m[1][2])),
		Vector3::Normalize(Vector3(rotB.m[2][0], rotB.m[2][1], rotB.m[2][2]))
	};

	// 15の分離軸
	Vector3 testAxes[15];
	int axisCount = 0;

	// OBB A, Bのローカル軸
	for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesA[i];
	for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axesB[i];

	// クロス積
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			testAxes[axisCount++] = Vector3::Normalize(Vector3::Cross(axesA[i], axesB[j]));
		}
	}

	Vector3 toCenter = obbB.center - obbA.center;

	for (int i = 0; i < 15; ++i)
	{
		const Vector3& axis = testAxes[i];
		if (axis.x == 0 && axis.y == 0 && axis.z == 0) continue; // 無効軸スキップ

		// 射影幅
		float aProj =
			std::abs(Vector3::Dot(axesA[0] * obbA.size.x, axis)) +
			std::abs(Vector3::Dot(axesA[1] * obbA.size.y, axis)) +
			std::abs(Vector3::Dot(axesA[2] * obbA.size.z, axis));

		float bProj =
			std::abs(Vector3::Dot(axesB[0] * obbB.size.x, axis)) +
			std::abs(Vector3::Dot(axesB[1] * obbB.size.y, axis)) +
			std::abs(Vector3::Dot(axesB[2] * obbB.size.z, axis));

		float distance = std::abs(Vector3::Dot(toCenter, axis));

		if (distance > aProj + bProj)
		{
			// 非衝突
			return false;
		}
	}
	// 衝突している場合、衝突した位置を設定
	ICollisionComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(obbA.center);
	bNonConst->SetCollisionPosition(obbB.center);

	// 衝突
	return true;
}

bool CollisionManager::CheckCollision(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	const AABB& aBox = a->GetAABB();
	const OBB& obb = b->GetOBB();

	// OBBの回転行列（各軸ベクトル）
	Matrix4x4 rot = obb.rotate;

	// OBBの軸ベクトル
	Vector3 axes[3] =
	{
		Vector3::Normalize(Vector3(rot.m[0][0], rot.m[0][1], rot.m[0][2])),
		Vector3::Normalize(Vector3(rot.m[1][0], rot.m[1][1], rot.m[1][2])),
		Vector3::Normalize(Vector3(rot.m[2][0], rot.m[2][1], rot.m[2][2]))
	};

	// AABBの中心座標をOBBのローカル空間に変換
	Vector3 toCenter = aBox.GetCenter() - obb.center;

	// AABBの半分のサイズ
	Vector3 aHalfSize = aBox.GetHalfSize();

	// 15の分離軸
	Vector3 testAxes[6];
	int axisCount = 0;

	// OBBの軸をテスト軸に追加
	for (int i = 0; i < 3; ++i) testAxes[axisCount++] = axes[i];

	// AABBの軸（X, Y, Z）をテスト軸に追加
	testAxes[axisCount++] = Vector3(1, 0, 0);
	testAxes[axisCount++] = Vector3(0, 1, 0);
	testAxes[axisCount++] = Vector3(0, 0, 1);

	// 各軸で分離軸テストを実行
	for (int i = 0; i < axisCount; ++i)
	{
		const Vector3& axis = testAxes[i];

		// AABBの射影幅
		float aProj = std::abs(Vector3::Dot(axis, Vector3(aHalfSize.x, 0.0f, 0.0f))) +
			std::abs(Vector3::Dot(axis, Vector3(0.0f, aHalfSize.y, 0.0f))) +
			std::abs(Vector3::Dot(axis, Vector3(0.0f, 0.0f, aHalfSize.z)));

		// OBBの射影幅
		float bProj = std::abs(Vector3::Dot(axes[0] * obb.size.x, axis)) +
			std::abs(Vector3::Dot(axes[1] * obb.size.y, axis)) +
			std::abs(Vector3::Dot(axes[2] * obb.size.z, axis));

		// AABBとOBBの中心の距離
		float distance = std::abs(Vector3::Dot(toCenter, axis));

		// 分離軸テスト（衝突していない場合は離れている）
		if (distance > aProj + bProj)
		{
			// 非衝突
			return false;
		}
	}

	// 衝突している場合、衝突した位置を設定
	ICollisionComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	ICollisionComponent* bNonConst = const_cast<OBBColliderComponent*>(b);
	aNonConst->SetCollisionPosition(aBox.GetCenter());
	aNonConst->SetCollisionPosition(obb.center);

	// 衝突している
	return true;
}

bool CollisionManager::CheckSubstepCollision(const AABBColliderComponent* a, const AABBColliderComponent* b)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f; // 1サブステップあたり最大移動量（調整可）

	// 両オブジェクトの移動情報を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	const AABB& bBox = b->GetAABB();

	// 静的判定（今フレームすでに重なってればOK）
	if (CheckCollision(a, b)) return true;

	// 両オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// より大きな移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	// constを外して操作を行えるようにする
	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	AABBColliderComponent* bNonConst = const_cast<AABBColliderComponent*>(b);

	for (int step = 1; step <= subStepCount; ++step)
	{
		float t = static_cast<float>(step) / subStepCount;

		// 両オブジェクトのサブステップ位置を計算
		Vector3 subPosA = MathUtils::Lerp(startA, endA, t);
		Vector3 subPosB = MathUtils::Lerp(startB, endB, t);

		// サブステップ位置での両AABBを作成
		AABB movedAABB_A(
			subPosA - aBox.GetHalfSize(),
			subPosA + aBox.GetHalfSize()
		);

		AABB movedAABB_B(
			subPosB - bBox.GetHalfSize(),
			subPosB + bBox.GetHalfSize()
		);

		// 一時的なコライダーコンポーネントを作成
		AABBColliderComponent tempA(nullptr);
		AABBColliderComponent tempB(nullptr);
		tempA.SetAABB(movedAABB_A);
		tempB.SetAABB(movedAABB_B);

		// 衝突判定
		if (CheckCollision(&tempA, &tempB))
		{
			// 衝突が検出された場合、サブステップ位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}

bool CollisionManager::CheckSubstepCollision(const OBBColliderComponent* a, const OBBColliderComponent* b)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f;

	// 両オブジェクトの移動情報を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	OBB aObb = a->GetOBB();
	OBB bObb = b->GetOBB();

	// 両オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// より大きな移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	// constを外して操作を行えるようにする
	OBBColliderComponent* aNonConst = const_cast<OBBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	for (int step = 0; step < subStepCount; ++step)
	{
		float t = static_cast<float>(step + 1) / subStepCount;

		// 両オブジェクトのサブステップ位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// 両OBBをサブステップ位置に仮想移動
		OBB movedOBB_A = aObb;
		OBB movedOBB_B = bObb;
		movedOBB_A.center = subPosA;
		movedOBB_B.center = subPosB;

		// 一時的なコライダーコンポーネントを作成
		OBBColliderComponent tempA(nullptr);
		OBBColliderComponent tempB(nullptr);
		tempA.SetOBB(movedOBB_A);
		tempB.SetOBB(movedOBB_B);

		if (CheckCollision(&tempA, &tempB))
		{
			// 衝突が検出された場合、サブステップ位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}

bool CollisionManager::CheckSubstepCollision(const AABBColliderComponent* a, const OBBColliderComponent* b)
{
	constexpr float MAX_STEP_DISTANCE = 1.0f;

	// 両オブジェクトの移動情報を取得
	Vector3 startA = a->GetPreviousPosition();
	Vector3 endA = a->GetOwner()->GetPosition();
	Vector3 startB = b->GetPreviousPosition();
	Vector3 endB = b->GetOwner()->GetPosition();

	const AABB& aBox = a->GetAABB();
	OBB bObb = b->GetOBB();

	// 両オブジェクトの移動距離を計算
	float distanceA = (endA - startA).Length();
	float distanceB = (endB - startB).Length();

	// より大きな移動距離に基づいてサブステップ数を決定
	float maxDistance = (std::max)(distanceA, distanceB);
	int subStepCount = (std::max)(1, static_cast<int>(std::ceil(maxDistance / MAX_STEP_DISTANCE)));

	// constを外して操作を行えるようにする
	AABBColliderComponent* aNonConst = const_cast<AABBColliderComponent*>(a);
	OBBColliderComponent* bNonConst = const_cast<OBBColliderComponent*>(b);

	for (int step = 0; step < subStepCount; ++step)
	{
		float t = static_cast<float>(step + 1) / subStepCount;

		// 両オブジェクトのサブステップ位置を計算
		Vector3 subPosA = startA + (endA - startA) * t;
		Vector3 subPosB = startB + (endB - startB) * t;

		// サブステップ位置でのOBBを作成
		OBB movedOBB = bObb;
		movedOBB.center = subPosB;

		// サブステップ位置でのAABBを作成
		Vector3 aHalf = aBox.GetHalfSize();
		AABB movedAABB(subPosA - aHalf, subPosA + aHalf);

		// 一時的なコライダーコンポーネントを作成
		AABBColliderComponent tempA(nullptr);
		OBBColliderComponent tempB(nullptr);
		tempA.SetAABB(movedAABB);
		tempB.SetOBB(movedOBB);

		if (CheckCollision(&tempA, &tempB))
		{
			// 衝突が検出された場合、サブステップ位置を記録
			aNonConst->SetCollisionPosition(subPosA);
			bNonConst->SetCollisionPosition(subPosB);
			return true;
		}
	}

	return false;
}

std::string CollisionManager::GetColliderTypeString(ColliderType type) const
{
	switch (type)
	{
	case ColliderType::AABB:
		return "AABB";
	case ColliderType::Sphere:
		return "Sphere";
	case ColliderType::OBB:
		return "OBB";
	}
	return "Unknown";
}

void CollisionManager::LogCollision(const std::string& phase, const ICollisionComponent* a, const ICollisionComponent* b)
{
#ifdef _DEBUG   // デバッグビルド時のみログを出力
	std::string tagA = a->GetOwner()->GetTag();
	std::string tagB = b->GetOwner()->GetTag();
	std::string typeAString = GetColliderTypeString(a->GetColliderType());
	std::string typeBString = GetColliderTypeString(b->GetColliderType());

	Logger::Log("| Collision " + phase + " " +
				(phase == "Exit" ? "<-" : (phase == "Enter" ? "->" : "=="))
				+ " | " + tagA + ": " + typeAString + ", " + tagB + ": " + typeBString + "\n");
#endif
}
