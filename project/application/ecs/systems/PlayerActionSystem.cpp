#include "PlayerActionSystem.h"

// engine
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/TagComponent.h" // ecs::TagComponent
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "engine/ecs/components/InstancedRenderComponent.h"
#include "engine/time/TimeManager.h"
#include "engine/ecs/system/SystemManager.h"
#include "engine/ecs/system/CollisionSystem.h"
#include "engine/ecs/components/LifetimeComponent.h"
#include "engine/manager/graphics/LineManager.h"
#include "math/VectorColorCodes.h"
#include "engine/effects/particle/ParticleManager.h"

// app components
#include "application/ecs/components/PlayerProgressionComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/DodgeComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "application/ecs/components/ProjectileComponent.h"
#include "application/ecs/components/InducedExplosionComponent.h"
#include "application/ecs/components/ImpactChargeComponent.h"
#include "application/ecs/CollisionConfig.h"

// app systems/managers
#include "input/Input.h"
#include "math/MathUtils.h"
#include "math/MatrixFunc.h"
#include "manager/scene/CameraManager.h"
#include "base/Camera.h"
#include "base/WinApp.h"
#include "application/effect/BulletTrailManager.h"
#include <algorithm>
#include <cmath>

void PlayerActionSystem::Update(Registry& registry)
{
	// プレイヤーエンティティを取得 (ecs::TagComponent::Type::Player で探す)
	auto tagView = registry.View<ecs::TagComponent>();
	if (!tagView) return;

	float dt = TimeManager::GetInstance().GetGameContext().deltaTime;
	if (dt <= 0.0f) dt = 0.0166f;

	for (uint32_t i = 0; i < tagView->GetSize(); ++i)
	{
		if (tagView->GetDataFromDenseIndex(i).type != ecs::TagComponent::Type::Player) continue;

		EntityID entity = tagView->GetEntityFromDenseIndex(i);

		// 必要なコンポーネントが揃っているか確認
		if (!registry.HasComponent<DodgeComponent>(entity)) continue;

		UpdateDodge(entity, registry, dt);

		// 回避中なら移動入力を受け付けない
		auto& dodge = registry.GetComponent<DodgeComponent>(entity);
		if (!dodge.isDodging_)
		{
			if (registry.HasComponent<TransformComponent>(entity) &&
				registry.HasComponent<ecs::StatusComponent>(entity))
			{
				UpdateMovement(entity, registry, dt);
			}

			if (registry.HasComponent<SkillComponent>(entity))
			{
				UpdateSkills(entity, registry, dt);
			}
		}
	}
}

void PlayerActionSystem::UpdateDodge(EntityID entity, Registry& registry, float dt)
{
	auto& dodge = registry.GetComponent<DodgeComponent>(entity);
	auto& trans = registry.GetComponent<TransformComponent>(entity);

	if (dodge.cooldownTimer_ > 0.0f) dodge.cooldownTimer_ -= dt;

	if (dodge.isDodging_)
	{
		dodge.timer_ -= dt;
		float progress = 1.0f - ((std::max)(0.0f, dodge.timer_) / dodge.kDuration);

		// イージングを用いた補間
		trans.localPosition_ = MathUtils::Lerp(dodge.startPosition_, dodge.targetPosition_, progress);
		trans.isDirty_ = true;

		if (dodge.timer_ <= 0.0f)
		{
			dodge.isDodging_ = false;
			dodge.cooldownTimer_ = dodge.kCooldown;
		}
	}
	else
	{
		// 回避入力
		if (Input::GetInstance()->TriggerKey(DIK_SPACE) && dodge.cooldownTimer_ <= 0.0f)
		{
			auto* input = Input::GetInstance();
			Vector3 inputDir = { 0, 0, 0 };
			if (input->PushKey(DIK_W)) inputDir.z += 1.0f;
			if (input->PushKey(DIK_S)) inputDir.z -= 1.0f;
			if (input->PushKey(DIK_A)) inputDir.x -= 1.0f;
			if (input->PushKey(DIK_D)) inputDir.x += 1.0f;

			if (inputDir.LengthSquared() > 0.01f)
			{
				if (cameraManager_)
				{
					float yaw = cameraManager_->GetActiveCamera()->GetRotate().y;
					Vector3 forward(sin(yaw), 0, cos(yaw));
					Vector3 right(cos(yaw), 0, -sin(yaw));
					dodge.direction_ = (forward * inputDir.z + right * inputDir.x).Normalize();
				}
				else
				{
					dodge.direction_ = inputDir.Normalize();
				}
			}
			else
			{
				// 入力がない場合は現在の向き
				float yaw = trans.localRotation_.y;
				dodge.direction_ = { sin(yaw), 0, cos(yaw) };
			}

			dodge.isDodging_ = true;
			dodge.timer_ = dodge.kDuration;
			dodge.startPosition_ = trans.localPosition_;
			dodge.targetPosition_ = dodge.startPosition_ + dodge.direction_ * dodge.kDistance;
		}
	}
}

void PlayerActionSystem::UpdateMovement(EntityID entity, Registry& registry, float dt)
{
	auto* input = Input::GetInstance();
	auto& trans = registry.GetComponent<TransformComponent>(entity);
	auto& status = registry.GetComponent<ecs::StatusComponent>(entity);

	Vector3 moveInput = { 0, 0, 0 };
	if (input->PushKey(DIK_W)) moveInput.z += 1.0f;
	if (input->PushKey(DIK_S)) moveInput.z -= 1.0f;
	if (input->PushKey(DIK_A)) moveInput.x -= 1.0f;
	if (input->PushKey(DIK_D)) moveInput.x += 1.0f;

	if (moveInput.LengthSquared() > 0.01f)
	{
		Vector3 finalDir = moveInput.Normalize();
		if (cameraManager_)
		{
			float yaw = cameraManager_->GetActiveCamera()->GetRotate().y;
			Vector3 forward(sin(yaw), 0, cos(yaw));
			Vector3 right(cos(yaw), 0, -sin(yaw));
			finalDir = (forward * moveInput.z + right * moveInput.x).Normalize();
		}

		float speed = status.moveSpeed_.GetValue();
		trans.localPosition_ = trans.localPosition_ + finalDir * speed * dt;
		trans.isDirty_ = true;
	}

	// マウス方向への回転
	if (cameraManager_)
	{
		Camera* camera = cameraManager_->GetActiveCamera();
		float mouseX = input->GetMouseX();
		float mouseY = input->GetMouseY();

		Matrix4x4 matViewport = MakeViewportMatrix(0, 0, WinApp::kClientWidth, WinApp::kClientHeight, 0, 1);
		Matrix4x4 matVPV = (camera->GetViewMatrix() * camera->GetProjectionMatrix()) * matViewport;
		Matrix4x4 matInverseVPV = Inverse(matVPV);

		Vector3 posNear = MathUtils::Transform({ mouseX, mouseY, 0.0f }, matInverseVPV);
		Vector3 posFar = MathUtils::Transform({ mouseX, mouseY, 1.0f }, matInverseVPV);

		Vector3 rayDir = (posFar - posNear).Normalize();
		if (std::abs(rayDir.y) > 0.0001f)
		{
			float t = (trans.localPosition_.y - posNear.y) / rayDir.y;
			Vector3 targetPos = posNear + rayDir * t;
			Vector3 lookDir = (targetPos - trans.localPosition_);
			lookDir.y = 0.0f;

			if (lookDir.LengthSquared() > 0.01f)
			{
				float targetYaw = atan2f(lookDir.x, lookDir.z);
				trans.localRotation_.y = MathUtils::LerpAngle(trans.localRotation_.y, targetYaw, 0.2f);
				trans.isDirty_ = true;
			}
		}
	}
}

void PlayerActionSystem::UpdateSkills(EntityID entity, Registry& registry, float dt)
{
	auto& skill = registry.GetComponent<SkillComponent>(entity);

	// タイマー更新
	if (skill.lmbTimer_ > 0.0f) skill.lmbTimer_ -= dt;
	if (skill.rmbTimer_ > 0.0f) skill.rmbTimer_ -= dt;
	if (skill.decoyTimer_ > 0.0f) skill.decoyTimer_ -= dt;
	if (skill.impactTimer_ > 0.0f) skill.impactTimer_ -= dt;
	if (skill.beamTimer_ > 0.0f) skill.beamTimer_ -= dt;

	UpdateLMB(entity, registry, dt);
	UpdateRMB(entity, registry, dt);
	UpdateQ(entity, registry, dt);
	UpdateE(entity, registry, dt);
	UpdateR(entity, registry, dt);

	// デコイの可視化 (デバッグ用)
	auto decoyView = registry.View<ecs::TagComponent>();
	if (decoyView)
	{
		for (uint32_t i = 0; i < decoyView->GetSize(); ++i)
		{
			if (decoyView->GetDataFromDenseIndex(i).type == ecs::TagComponent::Type::Decoy)
			{
				EntityID decoyEnt = decoyView->GetEntityFromDenseIndex(i);
				if (registry.HasComponent<TransformComponent>(decoyEnt))
				{
					auto& dTrans = registry.GetComponent<TransformComponent>(decoyEnt);
					LineManager::GetInstance()->DrawCube(dTrans.localPosition_, 1.0f, VectorColorCodes::Yellow);
				}
			}
		}
	}
}

void PlayerActionSystem::UpdateLMB(EntityID entity, Registry& registry, float)
{
	auto& skill = registry.GetComponent<SkillComponent>(entity);
	if (!skill.isLmbUnlocked_) return;
	if (skill.lmbTimer_ > 0.0f) return;

	if (Input::GetInstance()->IsMouseButtonPressed(0))
	{
		// 弾を生成
		auto& trans = registry.GetComponent<TransformComponent>(entity);
		float yaw = trans.localRotation_.y;
		Vector3 dir = { sin(yaw), 0, cos(yaw) };

		EntityID proj = registry.CreateEntity();
		Vector3 spawnPos = { trans.localPosition_.x, 0.5f, trans.localPosition_.z };
		registry.AddComponent<TransformComponent>(proj, { spawnPos, trans.localRotation_, {1.5f, 1.5f, 1.5f} });

		ProjectileComponent pc;
		pc.type_ = ProjectileComponent::Type::Lmb;
		pc.velocity_ = dir * 80.0f;
		pc.damage_ = 10.0f;
		pc.lifetime_ = 1.5f;
		pc.trailId_ = BulletTrailManager::GetInstance().RegisterBulletManual();
		registry.AddComponent<ProjectileComponent>(proj, pc);

		// Rendering
		InstancedRenderComponent render;
		render.modelName_ = "bullet";
		render.useInstancing_ = true;
		registry.AddComponent<InstancedRenderComponent>(proj, render);

		// コライダー設定 (BNS-Style: 振る舞いをデータとして持たせる)
		ecs::ColliderComponent col;
		col.type_ = ColliderType::Sphere;
		col.sphere_.radius = 0.4f;
		col.previousPosition_ = trans.localPosition_;
		col.isTrigger_ = true; // 物理的に押し返さない

		// フィルタリング設定
		col.layer = CollisionLayer::PlayerBullet;
		col.mask = CollisionLayer::Enemy | CollisionLayer::Obstacle;

		// 衝突応答
		col.onCollisionEnter = [this, &registry, proj](const ecs::CollisionPartnerInfo& other) {
			// 弾は Enemy または Obstacle に当たったら自身を消す
			if (registry.HasComponent<ecs::ColliderComponent>(other.entity))
			{
				auto& otherCol = registry.GetComponent<ecs::ColliderComponent>(other.entity);
				if (otherCol.layer & (CollisionLayer::Enemy))
				{
					// 誘爆スタックの加算
					if (registry.HasComponent<ecs::InducedExplosionComponent>(other.entity))
					{
						auto& stack = registry.GetComponent<ecs::InducedExplosionComponent>(other.entity);
						stack.count_ += 3;
						if (stack.count_ >= ecs::InducedExplosionComponent::kMaxCount)
						{
							SpawnExplosion(other.entity, registry);
						}
					}

					// ヒットエフェクトの再生
					if (registry.HasComponent<TransformComponent>(proj))
					{
						auto& bulletTrans = registry.GetComponent<TransformComponent>(proj);
						ParticleManager::GetInstance()->Play("hit_effect_ver2", bulletTrans.localPosition_);
					}

					// ダメージ処理 (80ダメージに調整)
					if (registry.HasComponent<ecs::StatusComponent>(other.entity))
					{
						auto& status = registry.GetComponent<ecs::StatusComponent>(other.entity);
						status.hp_.SetBase(status.hp_.GetBase() - 80.0f);
					}

					registry.DestroyEntityDeferred(proj);
				}
				else if (otherCol.layer & (CollisionLayer::Obstacle))
				{
					registry.DestroyEntityDeferred(proj);
				}
			}
			};

		registry.AddComponent<ecs::ColliderComponent>(proj, col);
		registry.AddComponent<CollisionResponseComponent>(proj, {});

		skill.lmbTimer_ = skill.kLmbCooldown;
	}
}

void PlayerActionSystem::UpdateRMB(EntityID entity, Registry& registry, float)
{
	auto& skill = registry.GetComponent<SkillComponent>(entity);
	if (!skill.isRmbUnlocked_) return;
	if (skill.rmbTimer_ > 0.0f) return;

	if (Input::GetInstance()->IsMouseButtonPressed(2)) // 右クリック
	{
		auto& trans = registry.GetComponent<TransformComponent>(entity);
		float yaw = trans.localRotation_.y;
		Vector3 dir = { sin(yaw), 0, cos(yaw) };

		EntityID proj = registry.CreateEntity();
		Vector3 spawnPos = { trans.localPosition_.x, 0.5f, trans.localPosition_.z };
		registry.AddComponent<TransformComponent>(proj, { spawnPos, trans.localRotation_, {1.0f, 1.0f, 1.0f} });

		ProjectileComponent pc;
		pc.type_ = ProjectileComponent::Type::Rmb;
		pc.velocity_ = dir * 60.0f;
		pc.damage_ = 2.0f; // 低火力
		pc.lifetime_ = 1.0f;
		pc.trailId_ = BulletTrailManager::GetInstance().RegisterBulletManual();
		registry.AddComponent<ProjectileComponent>(proj, pc);

		// ecs::TagComponent を追加
		ecs::TagComponent tag;
		tag.type = ecs::TagComponent::Type::Bullet;
		registry.AddComponent<ecs::TagComponent>(proj, tag);

		InstancedRenderComponent render;
		render.modelName_ = "bullet";
		render.useInstancing_ = true;
		render.isVisible_ = false; // パーティクルのみにする
		registry.AddComponent<InstancedRenderComponent>(proj, render);

		ecs::ColliderComponent col;
		col.type_ = ColliderType::Sphere;
		col.sphere_.radius = 0.5f;
		col.isTrigger_ = true;
		col.layer = CollisionLayer::PlayerBullet;
		col.mask = CollisionLayer::Enemy;

		// 連鎖ロジック用キャプチャ
		SystemManager* sysMgr = systemManager_;

		col.onCollisionEnter = [this, &registry, proj, sysMgr](const ecs::CollisionPartnerInfo& other) {
			if (registry.HasComponent<ecs::TagComponent>(other.entity) &&
				registry.GetComponent<ecs::TagComponent>(other.entity).type == ecs::TagComponent::Type::Enemy)
			{
				// 命中地点
				Vector3 hitPos = registry.GetComponent<TransformComponent>(other.entity).localPosition_;

				// 近くの敵を検索して連鎖
				auto colSys = sysMgr->GetSystem<CollisionSystem>();
				if (colSys)
				{
					int chainCount = 0;
					const int kMaxChain = 5;
					colSys->QueryNearbyEntities(hitPos, 8.0f, [this, &registry, hitPos, &chainCount, kMaxChain](EntityID victim) {
						if (chainCount >= kMaxChain) return;
						if (victim == kInvalidEntity) return;

						if (registry.HasComponent<ecs::TagComponent>(victim) &&
							registry.GetComponent<ecs::TagComponent>(victim).type == ecs::TagComponent::Type::Enemy)
						{
							// 距離の精査 (8.0f以内)
							Vector3 victimPos = registry.GetComponent<TransformComponent>(victim).localPosition_;
							float distSq = (victimPos - hitPos).LengthSquared();
							if (distSq > 8.0f * 8.0f) return;

							// 誘爆スタックの加算
							if (registry.HasComponent<ecs::InducedExplosionComponent>(victim))
							{
								auto& stack = registry.GetComponent<ecs::InducedExplosionComponent>(victim);
								stack.count_++;
								if (stack.count_ >= ecs::InducedExplosionComponent::kMaxCount)
								{
									SpawnExplosion(victim, registry);
								}
							}

							// ダメージ処理 (15ダメージに調整)
							if (registry.HasComponent<ecs::StatusComponent>(victim))
							{
								auto& status = registry.GetComponent<ecs::StatusComponent>(victim);
								status.hp_.SetBase(status.hp_.GetBase() - 15.0f);
							}

							// 雷描画 (とりあえずラインマネージャー)
							LineManager::GetInstance()->DrawLine(hitPos, victimPos, VectorColorCodes::Cyan);

							chainCount++;
						}
												});
				}
				// 最初に当たった敵のスタックとダメージ処理
				if (registry.HasComponent<ecs::InducedExplosionComponent>(other.entity))
				{
					auto& stack = registry.GetComponent<ecs::InducedExplosionComponent>(other.entity);
					stack.count_++;
					if (stack.count_ >= ecs::InducedExplosionComponent::kMaxCount)
					{
						SpawnExplosion(other.entity, registry);
					}
				}

				// ヒットエフェクトの再生
				if (registry.HasComponent<TransformComponent>(proj))
				{
					auto& bulletTrans = registry.GetComponent<TransformComponent>(proj);
					ParticleManager::GetInstance()->Play("hit_effect_ver2", bulletTrans.localPosition_);
				}

				// ダメージ処理 (15ダメージに調整)
				if (registry.HasComponent<ecs::StatusComponent>(other.entity))
				{
					auto& status = registry.GetComponent<ecs::StatusComponent>(other.entity);
					status.hp_.SetBase(status.hp_.GetBase() - 15.0f);
				}

				registry.DestroyEntityDeferred(proj);
			}
			};

		registry.AddComponent<ecs::ColliderComponent>(proj, col);
		registry.AddComponent<CollisionResponseComponent>(proj, {});

		skill.rmbTimer_ = skill.kRmbCooldown;
	}
}

void PlayerActionSystem::UpdateQ(EntityID entity, Registry& registry, float)
{
	auto& skill = registry.GetComponent<SkillComponent>(entity);
	if (!skill.isDecoyUnlocked_) return;
	if (skill.decoyTimer_ > 0.0f) return;

	if (Input::GetInstance()->TriggerKey(DIK_Q))
	{
		auto& trans = registry.GetComponent<TransformComponent>(entity);
		float yaw = trans.localRotation_.y;
		Vector3 forward = { sin(yaw), 0, cos(yaw) };

		EntityID decoy = registry.CreateEntity();
		Vector3 spawnPos = trans.localPosition_ + forward * 3.0f;
		registry.AddComponent<TransformComponent>(decoy, { spawnPos, {0,0,0}, {1,1,1} });

		ecs::TagComponent tag;
		tag.type = ecs::TagComponent::Type::Decoy;
		registry.AddComponent<ecs::TagComponent>(decoy, tag);

		InstancedRenderComponent render;
		render.modelName_ = "cube"; // 仮
		render.useInstancing_ = true;
		registry.AddComponent<InstancedRenderComponent>(decoy, render);

		registry.AddComponent<LifetimeComponent>(decoy, { 0.0f, 5.0f });

		skill.decoyTimer_ = skill.kDecoyCooldown;
	}
}

void PlayerActionSystem::UpdateE(EntityID entity, Registry& registry, float)
{
	auto& skill = registry.GetComponent<SkillComponent>(entity);
	if (!skill.isImpactUnlocked_) return;
	if (skill.impactTimer_ > 0.0f) return;

	if (Input::GetInstance()->TriggerKey(DIK_E))
	{
		auto& trans = registry.GetComponent<TransformComponent>(entity);

		// Eスキルパーティクルの再生
		ParticleManager::GetInstance()->Play("E_skill", trans.localPosition_);

		// インパクトエンティティ（透明な衝撃波判定）
		EntityID impact = registry.CreateEntity();
		registry.AddComponent<TransformComponent>(impact, { trans.localPosition_, {0,0,0}, {1,1,1} });
		registry.AddComponent<LifetimeComponent>(impact, { 0.0f, 0.1f }); // 1フレームに近い

		ecs::ColliderComponent col;
		col.type_ = ColliderType::Sphere;
		col.sphere_.radius = 15.0f;
		col.isTrigger_ = true;
		col.layer = CollisionLayer::PlayerBullet;
		col.mask = CollisionLayer::Enemy;

		SystemManager* sysMgr = systemManager_;

		col.onCollisionEnter = [this, &registry](const ecs::CollisionPartnerInfo& other) {
			if (registry.HasComponent<ecs::TagComponent>(other.entity) &&
				registry.GetComponent<ecs::TagComponent>(other.entity).type == ecs::TagComponent::Type::Enemy)
			{
				// 誘爆スタックの付与・更新
				if (!registry.HasComponent<ecs::InducedExplosionComponent>(other.entity))
				{
					registry.AddComponent<ecs::InducedExplosionComponent>(other.entity, { 1 });
				}
				else
				{
					auto& stack = registry.GetComponent<ecs::InducedExplosionComponent>(other.entity);
					stack.count_++;
					if (stack.count_ >= ecs::InducedExplosionComponent::kMaxCount)
					{
						SpawnExplosion(other.entity, registry);
					}
				}

				// ダメージ処理 (20ダメージ追加)
				if (registry.HasComponent<ecs::StatusComponent>(other.entity))
				{
					auto& status = registry.GetComponent<ecs::StatusComponent>(other.entity);
					status.hp_.SetBase(status.hp_.GetBase() - 20.0f);
				}
			}
			};

		registry.AddComponent<ecs::ColliderComponent>(impact, col);
		registry.AddComponent<CollisionResponseComponent>(impact, {});

		skill.impactTimer_ = skill.kImpactCooldown;
	}
}

void PlayerActionSystem::UpdateR(EntityID entity, Registry& registry, float)
{
	auto& skill = registry.GetComponent<SkillComponent>(entity);
	if (!skill.isBeamUnlocked_) return;
	// 実装予定
}

void PlayerActionSystem::SpawnExplosion(EntityID sourceEntity, Registry& registry)
{
	if (!registry.IsAlive(sourceEntity)) return;
	if (!registry.HasComponent<TransformComponent>(sourceEntity)) return;

	Vector3 expPos = registry.GetComponent<TransformComponent>(sourceEntity).localPosition_;

	// 爆発エンティティ（透明な衝撃波判定）
	EntityID explosion = registry.CreateEntity();
	registry.AddComponent<TransformComponent>(explosion, { expPos, {0,0,0}, {1,1,1} });
	registry.AddComponent<LifetimeComponent>(explosion, { 0.0f, 0.1f });

	ecs::ColliderComponent col;
	col.type_ = ColliderType::Sphere;
	col.sphere_.radius = ecs::InducedExplosionComponent::kExplosionRadius;
	col.isTrigger_ = true;
	col.layer = CollisionLayer::PlayerBullet;
	col.mask = CollisionLayer::Enemy;

	// 爆発のコールバック：周囲の敵にダメージ（または即死）
	col.onCollisionEnter = [&registry, expPos](const ecs::CollisionPartnerInfo& other) {
		if (registry.HasComponent<ecs::TagComponent>(other.entity) &&
			registry.GetComponent<ecs::TagComponent>(other.entity).type == ecs::TagComponent::Type::Enemy)
		{
			// 爆発演出の再生
			ParticleManager::GetInstance()->Play("E_explosion", expPos);
			// 爆発に巻き込まれた敵に大ダメージを与える (500ダメージ)
			if (registry.HasComponent<ecs::StatusComponent>(other.entity))
			{
				auto& status = registry.GetComponent<ecs::StatusComponent>(other.entity);
				status.hp_.SetBase(status.hp_.GetBase() - 500.0f);
				if (status.hp_.GetBase() <= 0.0f) status.isAlive_ = false;
			}
		}
		};

	registry.AddComponent<ecs::ColliderComponent>(explosion, col);
	registry.AddComponent<CollisionResponseComponent>(explosion, {});

	// 元の敵自身にもダメージ（または破棄）
	if (registry.HasComponent<ecs::StatusComponent>(sourceEntity))
	{
		auto& status = registry.GetComponent<ecs::StatusComponent>(sourceEntity);
		status.hp_.SetBase(status.hp_.GetBase() - 500.0f);
		if (status.hp_.GetBase() <= 0.0f) status.isAlive_ = false;
		// 爆発演出の再生
		ParticleManager::GetInstance()->Play("E_explosion", expPos);
	}

	registry.RemoveComponent<ecs::InducedExplosionComponent>(sourceEntity);

	// デバッグ表示
	LineManager::GetInstance()->DrawSphere(expPos, ecs::InducedExplosionComponent::kExplosionRadius, VectorColorCodes::Red);
}
