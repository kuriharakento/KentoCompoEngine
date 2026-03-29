#define NOMINMAX
#include "CollisionSystem.h"
#include <Windows.h>
#include <algorithm>
#include <numeric>
#include <execution>
#include <mutex>
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/HierarchyComponent.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "engine/gameobject/component/collision/CollisionAlgorithm.h"
#include "math/AABB.h"
#include "application/ecs/components/ObstacleComponent.h"
#include "engine/ecs/components/MovementComponent.h"
#include "math/VectorColorCodes.h"
#include "engine/manager/graphics/LineManager.h"
#include "math/MathUtils.h"
#include "application/ecs/CollisionConfig.h" // 新しいレイヤー定義

thread_local CollisionSystem::ThreadLocalContext tlContext;

CollisionSystem::CollisionSystem()
{
	grid_ = std::make_unique<LinearSpatialHash>(10.0f);
}

void CollisionSystem::UpdatePreviousPositions(Registry& registry)
{
	if (!registry.HasComponentArray<ColliderComponent>()) return;

	auto& colliderArray = registry.GetArray<ColliderComponent>();
	auto& transformArray = registry.GetArray<TransformComponent>();

	const uint32_t count = colliderArray.GetSize();
	if (colliderIndices_.size() < count) colliderIndices_.resize(count);
	std::iota(colliderIndices_.begin(), colliderIndices_.begin() + count, 0);

	std::for_each(std::execution::par, colliderIndices_.begin(), colliderIndices_.begin() + count, [&](uint32_t i) {
		EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
		if (!transformArray.HasComponent(entity)) return;

		auto& collider = colliderArray.GetDataFromDenseIndex(i);
		auto& transform = transformArray.GetData(entity);
		collider.previousPosition_ = MathUtils::GetTranslateFromMatrix(transform.worldMatrix_);
				  });
}

void CollisionSystem::Update(Registry& registry)
{
	// --- 1. 定期的なクレンジング ---
	if (registry.HasComponentArray<CollisionResponseComponent>())
	{
		auto& responseArray = registry.GetArray<CollisionResponseComponent>();
		for (uint32_t i = 0; i < responseArray.GetSize(); ++i)
		{
			responseArray.GetDataFromDenseIndex(i).ClearFrameEvents();
		}
	}

	if (registry.HasComponentArray<MovementComponent>())
	{
		auto& moveArray = registry.GetArray<MovementComponent>();
		for (uint32_t j = 0; j < moveArray.GetSize(); ++j)
		{
			moveArray.GetDataFromDenseIndex(j).isGrounded_ = false;
		}
	}

	if (!registry.HasComponentArray<ColliderComponent>() || !registry.HasComponentArray<TransformComponent>())
	{
		return;
	}

	auto& colliderArray = registry.GetArray<ColliderComponent>();
	auto& transformArray = registry.GetArray<TransformComponent>();
	const uint32_t colliderCount = colliderArray.GetSize();

	if (colliderIndices_.size() < colliderCount) colliderIndices_.resize(colliderCount);
	std::iota(colliderIndices_.begin(), colliderIndices_.begin() + colliderCount, 0);

	// --- 2. ワールド形状の更新 (Parallel) ---
	std::for_each(std::execution::par, colliderIndices_.begin(), colliderIndices_.begin() + colliderCount, [&](uint32_t i) {
		EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
		auto& collider = colliderArray.GetDataFromDenseIndex(i);
		if (!collider.isActive_ || !transformArray.HasComponent(entity)) return;

		auto& transform = transformArray.GetData(entity);

		// 世界行列の最新化（DirtyチェックはScene側で保証されるが念のため）
		Matrix4x4 worldMat = transform.worldMatrix_;

		Vector3 worldPos = MathUtils::GetTranslateFromMatrix(worldMat);
		Matrix4x4 worldRot = MathUtils::GetMatrixRotate(worldMat);
		Vector3 worldScale = MathUtils::GetScaleFromMatrix(worldMat);
		Vector3 rotatedOffset = MathUtils::TransformNormal(collider.offset_, worldRot);
		Vector3 center = worldPos + rotatedOffset;

		if (collider.type_ == ColliderType::Sphere)
		{
			float maxS = (std::max)({ worldScale.x, worldScale.y, worldScale.z });
			collider.worldSphere_ = Sphere(center, collider.sphere_.radius * maxS);
			float r = collider.worldSphere_.radius;
			collider.worldAabb_ = AABB(center - Vector3(r, r, r), center + Vector3(r, r, r));
		}
		else if (collider.type_ == ColliderType::OBB)
		{
			collider.worldObb_.center = center;
			collider.worldObb_.size = worldScale * ((collider.obb_.size.LengthSquared() > 1e-6f) ? collider.obb_.size : Vector3(1, 1, 1));
			collider.worldObb_.rotate = worldRot;

			// OBBを包むAABBの計算
			Vector3 halfEx = {
				std::abs(worldRot.m[0][0] * collider.worldObb_.size.x) + std::abs(worldRot.m[1][0] * collider.worldObb_.size.y) + std::abs(worldRot.m[2][0] * collider.worldObb_.size.z),
				std::abs(worldRot.m[0][1] * collider.worldObb_.size.x) + std::abs(worldRot.m[1][1] * collider.worldObb_.size.y) + std::abs(worldRot.m[2][1] * collider.worldObb_.size.z),
				std::abs(worldRot.m[0][2] * collider.worldObb_.size.x) + std::abs(worldRot.m[1][2] * collider.worldObb_.size.y) + std::abs(worldRot.m[2][2] * collider.worldObb_.size.z)
			};
			collider.worldAabb_ = AABB(center - halfEx, center + halfEx);
		}
		else
		{
			Vector3 halfSize = collider.aabb_.GetHalfSize() * worldScale;
			collider.worldAabb_ = AABB(center - halfSize, center + halfSize);
		}

		if (collider.useSubstep_)
		{
			Vector3 prevCenter = collider.previousPosition_ + rotatedOffset;
			AABB prevAabb(prevCenter - collider.worldAabb_.GetHalfSize(), prevCenter + collider.worldAabb_.GetHalfSize());
			collider.worldAabb_.min_ = Vector3::Min(collider.worldAabb_.min_, prevAabb.min_);
			collider.worldAabb_.max_ = Vector3::Max(collider.worldAabb_.max_, prevAabb.max_);
		}
				  });

	// --- 3. 空間ハッシュの再構築 (Serial Build) ---
	grid_->Clear();
	for (uint32_t i = 0; i < colliderCount; ++i)
	{
		auto& col = colliderArray.GetDataFromDenseIndex(i);
		if (col.isActive_) grid_->AddCount(col.worldAabb_);
	}
	grid_->BuildOffsets();
	for (uint32_t i = 0; i < colliderCount; ++i)
	{
		EntityID ent = colliderArray.GetEntityFromDenseIndex(i);
		auto& col = colliderArray.GetDataFromDenseIndex(i);
		if (col.isActive_) grid_->AddEntity(ent, col.worldAabb_);
	}

	// --- 4. 衝突詳細判定 (Parallel) ---
	DetectCollisions(registry);

	// --- 5. 結果のマージとレスポンス (Serial) ---
	ResolveCollisions(registry);

	// --- 6. 疎結合な衝突イベントコールバック (Serial) ---
	if (registry.HasComponentArray<CollisionResponseComponent>())
	{
		auto& respArray = registry.GetArray<CollisionResponseComponent>();
		for (uint32_t i = 0; i < respArray.GetSize(); ++i)
		{
			EntityID entity = respArray.GetEntityFromDenseIndex(i);
			auto& res = respArray.GetDataFromDenseIndex(i);
			if (!registry.HasComponent<ColliderComponent>(entity)) continue;
			auto& collider = registry.GetComponent<ColliderComponent>(entity);

			// Enter / Stay
			for (EntityID current : res.currentCollisions_)
			{
				if (res.previousCollisions_.count(current))
				{
					if (collider.onCollisionStay) collider.onCollisionStay({ current });
				}
				else
				{
					if (collider.onCollisionEnter)
					{
						collider.onCollisionEnter({ current });
					}
				}
			}
			// Exit
			for (EntityID prev : res.previousCollisions_)
			{
				if (!res.currentCollisions_.count(prev))
				{
					if (collider.onCollisionExit) collider.onCollisionExit({ prev });
				}
			}
		}
	}
}

void CollisionSystem::DetectCollisions(Registry& registry)
{
	auto& colliderArray = registry.GetArray<ColliderComponent>();
	auto* obstacleArray = registry.HasComponentArray<ObstacleComponent>() ? &registry.GetArray<ObstacleComponent>() : nullptr;
	const uint32_t colliderCount = colliderArray.GetSize();

	collisions_.clear();

	std::for_each(std::execution::par, colliderIndices_.begin(), colliderIndices_.begin() + colliderCount, [&](uint32_t i) {
		EntityID entityA = colliderArray.GetEntityFromDenseIndex(i);
		auto& colA = colliderArray.GetDataFromDenseIndex(i);
		if (!colA.isActive_) return;

		// スレッドローカルバッファ
		auto& neighbors = tlContext.neighbors;
		neighbors.clear();

		grid_->QueryNearby(colA.worldAabb_, [&](EntityID neighbor) {
			if (entityA < neighbor) neighbors.push_back(neighbor);
						   });

		if (neighbors.empty()) return;
		std::sort(neighbors.begin(), neighbors.end());
		neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());

		bool isAObs = obstacleArray && obstacleArray->HasComponent(entityA);

		for (EntityID entityB : neighbors)
		{
			if (!registry.IsAlive(entityB)) continue;
			auto& colB = colliderArray.GetData(entityB);
			if (!colB.isActive_) continue;

			// --- 汎用ビットマスクフィルタ (BNS-Standard) ---
			// 互いに相手をマスクしていなければ判定しない
			if (!(colA.mask & colB.layer) && !(colB.mask & colA.layer)) continue;

			bool isBObs = obstacleArray && obstacleArray->HasComponent(entityB);
			if (isAObs && isBObs) continue; // 障害物同士は判定しない

			// LOD判定（エンジン既定）
			collisionAlgorithm::CollisionLOD lod = collisionAlgorithm::CollisionLOD::Precise;
			if (colA.useSubstep_ || colB.useSubstep_) lod = collisionAlgorithm::CollisionLOD::CCD;
			// 双方 Sphere なら無条件で高速判定
			else if (colA.type_ == ColliderType::Sphere && colB.type_ == ColliderType::Sphere) lod = collisionAlgorithm::CollisionLOD::Sphere;

			bool hit = false;
			Vector3 mtv = { 0,0,0 };
			bool needsMTV = (!colA.isTrigger_ && !colB.isTrigger_) && (isAObs || isBObs);

			if (lod == collisionAlgorithm::CollisionLOD::Sphere)
			{
				hit = collisionAlgorithm::CheckSpherevsSphere(colA.worldSphere_, colB.worldSphere_);
				if (hit && needsMTV) collisionAlgorithm::CheckSpherevsSphereMTV(colA.worldSphere_, colB.worldSphere_, mtv);
			}
			else
			{
				auto funcTable = (lod == collisionAlgorithm::CollisionLOD::CCD) ? collisionAlgorithm::kCCDFuncTable : collisionAlgorithm::kCollisionFuncTable;
				auto func = funcTable[(int)colA.type_][(int)colB.type_];
				if (func) hit = func(colA, colB, needsMTV ? &mtv : nullptr);
			}

			if (hit)
			{
				std::lock_guard<std::mutex> lock(mergeMutex_);
				collisions_.push_back({ entityA, entityB, needsMTV, mtv });

				// 判定ログ (Debug)
				// char buf[128];
				// sprintf_s(buf, "[Collision] Detected: %u vs %u\n", entityA, entityB);
				// OutputDebugStringA(buf);
			}
		}
				  });
}

void CollisionSystem::ResolveCollisions(Registry& registry)
{
	auto& transformArray = registry.GetArray<TransformComponent>();
	auto* obstacleArray = registry.HasComponentArray<ObstacleComponent>() ? &registry.GetArray<ObstacleComponent>() : nullptr;
	auto* responseArray = registry.HasComponentArray<CollisionResponseComponent>() ? &registry.GetArray<CollisionResponseComponent>() : nullptr;

	for (const auto& ev : collisions_)
	{
		if (!registry.IsAlive(ev.a) || !registry.IsAlive(ev.b)) continue;

		// 状態追跡用に登録
		if (responseArray)
		{
			if (responseArray->HasComponent(ev.a)) responseArray->GetData(ev.a).currentCollisions_.insert(ev.b);
			if (responseArray->HasComponent(ev.b)) responseArray->GetData(ev.b).currentCollisions_.insert(ev.a);
		}

		// 物理的な押し戻し (Solid Response)
		if (ev.isSolid)
		{
			bool isAObs = obstacleArray && obstacleArray->HasComponent(ev.a);
			EntityID charEnt = isAObs ? ev.b : ev.a;
			Vector3 finalMtv = isAObs ? ev.mtv : -ev.mtv;

			if (transformArray.HasComponent(charEnt))
			{
				auto& trans = transformArray.GetData(charEnt);
				Vector3 currentPos = MathUtils::GetTranslateFromMatrix(trans.worldMatrix_);
				Vector3 newWorldPos = currentPos + finalMtv;

				// ワールト行列を直接更新
				trans.worldMatrix_.m[3][0] = newWorldPos.x;
				trans.worldMatrix_.m[3][1] = newWorldPos.y;
				trans.worldMatrix_.m[3][2] = newWorldPos.z;

				trans.localPosition_ = newWorldPos;
				trans.isDirty_ = true;

				if (registry.HasComponent<MovementComponent>(charEnt) && finalMtv.y > 0.05f)
				{
					registry.GetComponent<MovementComponent>(charEnt).isGrounded_ = true;
				}
			}
		}

		// --- 秘伝のタレ: ここにあった DestroyEntityDeferred 系のゲームロジックは撤廃 ---
		// それらは各エンティティの onCollisionEnter 等のコールバックで行う。
	}
}

void CollisionSystem::Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
#ifdef _DEBUG
	auto* lineManager = LineManager::GetInstance();
	if (!lineManager) return;

	if (registry.HasComponentArray<ColliderComponent>())
	{
		auto& colliderArray = registry.GetArray<ColliderComponent>();
		for (uint32_t i = 0; i < colliderArray.GetSize(); ++i)
		{
			EntityID entity = colliderArray.GetEntityFromDenseIndex(i);
			auto& collider = colliderArray.GetDataFromDenseIndex(i);
			if (!collider.isActive_ || !registry.HasComponent<TransformComponent>(entity)) continue;

			Vector4 color = collider.isTrigger_ ? VectorColorCodes::Lime : VectorColorCodes::Cyan;
			switch (collider.type_)
			{
			case ColliderType::AABB:   lineManager->DrawAABB(collider.worldAabb_, color); break;
			case ColliderType::Sphere: lineManager->DrawSphere(collider.worldSphere_.center, collider.worldSphere_.radius, color); break;
			case ColliderType::OBB:    lineManager->DrawOBB(collider.worldObb_, color); break;
			}
		}
	}
#endif
}
