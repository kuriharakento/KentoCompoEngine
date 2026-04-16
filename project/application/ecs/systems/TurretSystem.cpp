#include "TurretSystem.h"
#include "engine/ecs/Registry.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/TagComponent.h"
#include "engine/ecs/system/SystemManager.h"
#include "engine/ecs/system/CollisionSystem.h"
#include "engine/time/TimeManager.h"
#include "engine/effects/particle/ParticleManager.h"
#include "application/ecs/components/TurretComponent.h"
#include "application/ecs/components/SkillComponent.h"
#include "application/ecs/components/StatusComponent.h"
#include "application/ecs/components/DamageStackComponent.h"
#include "application/ecs/components/InducedExplosionComponent.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include "application/ecs/components/ProjectileComponent.h"
#include "application/effect/BulletTrailManager.h"
#include "application/effect/HomingTrailManager.h"
#include "application/ecs/CollisionConfig.h"
#include "engine/ecs/components/ColliderComponent.h"
#include "engine/ecs/components/CollisionResponseComponent.h"
#include "engine/audio/Audio.h"
#include "engine/math/MathUtils.h"
#include "engine/ecs/components/InstancedRenderComponent.h"
#include "engine/effects/particle/ParticleEffect.h"
#include "engine/effects/particle/ParticleManager.h"
#include "engine/effects/particle/module/spawn/InitialModules.h"
#include "engine/effects/particle/module/spawn/SpawnShapeModules.h"

void TurretSystem::Update(Registry& registry)
{
	auto view = registry.View<TurretComponent>();
	if (!view) return;

	float dt = TimeManager::GetInstance().GetGameContext().deltaTime;

	for (uint32_t i = 0; i < view->GetSize(); ++i)
	{
		EntityID turretEntity = view->GetEntityFromDenseIndex(i);
		auto& turret = view->GetDataFromDenseIndex(i);

		if (!registry.HasComponent<TransformComponent>(turretEntity)) continue;
		auto& turretTrans = registry.GetComponent<TransformComponent>(turretEntity);

		// タイマー更新
		turret.fireTimer_ -= dt;
		if (turret.fireTimer_ > 0.0f) continue;

		// オーナーのスキル派生タイプとバフ状態を取得
		SkillSpecialChoice specialType = SkillSpecialChoice::None;
		bool isBuffActive = false;
		float qRateMult = 1.0f;
		float eLaserRateMult = 1.0f;
		float eSalvoDmgMult = 1.0f;

		if (turret.owner_ != kInvalidEntity && registry.HasComponent<SkillComponent>(turret.owner_))
		{
			auto& skill = registry.GetComponent<SkillComponent>(turret.owner_);
			specialType = skill.special_;
			isBuffActive = skill.isTurretBuffActive_;
			qRateMult = skill.qTurretFireRateMult_;
			eLaserRateMult = skill.eLaserFireRateMult_;
			eSalvoDmgMult = skill.eSalvoDamageMult_;
		}

		// プラズマレーザーモード（チャネル方式）の個別処理
		if (isBuffActive && specialType == SkillSpecialChoice::PlasmaLaser)
		{
			UpdateLaserBeam(turretEntity, turret, registry, dt);
			continue; // 通常の射撃処理はスキップ
		}
		else
		{
			// レーザーモードでない場合はビームがあれば削除
			if (turret.activeBeam_ != kInvalidEntity)
			{
				registry.DestroyEntityDeferred(turret.activeBeam_);
				turret.activeBeam_ = kInvalidEntity;
			}
			// エフェクトも停止
			if (turret.laserEffect_)
			{
				turret.laserEffect_->Stop();
				turret.laserEffect_->SetAutoRemove(true); // あとはマネージャーの自動削除に任せる
				turret.laserEffect_ = nullptr;
			}
		}

		// 最も近い敵を検索
		EntityID closestEnemy = kInvalidEntity;
		float closestDistSq = turret.range_ * turret.range_;

		auto tagView = registry.View<ecs::TagComponent>();
		if (tagView)
		{
			for (uint32_t j = 0; j < tagView->GetSize(); ++j)
			{
				if (tagView->GetDataFromDenseIndex(j).type != ecs::TagComponent::Type::Enemy) continue;

				EntityID enemy = tagView->GetEntityFromDenseIndex(j);
				if (!registry.HasComponent<TransformComponent>(enemy)) continue;

				auto& enemyTrans = registry.GetComponent<TransformComponent>(enemy);
				float distSq = (enemyTrans.localPosition_ - turretTrans.localPosition_).LengthSquared();
				if (distSq < closestDistSq)
				{
					closestDistSq = distSq;
					closestEnemy = enemy;
				}
			}
		}

		if (closestEnemy == kInvalidEntity) continue;

		// 射撃タイマーリセット（アップグレードとバフによるレート変更）
		float currentInterval = turret.fireInterval_ * qRateMult;
		if (isBuffActive && specialType == SkillSpecialChoice::PlasmaLaser)
		{
			currentInterval *= eLaserRateMult; // レーザー時はさらに短縮
		}
		turret.fireTimer_ = currentInterval;

		// 弾（プロジェクタイル）の生成
		EntityID proj = registry.CreateEntity();

		Vector3 startPos = turretTrans.localPosition_;
		// 少し浮かせた位置から発射
		startPos.y += 0.5f;

		// ターゲットへの方向を計算
		Vector3 enemyPos = registry.GetComponent<TransformComponent>(closestEnemy).localPosition_;
		enemyPos.y += 0.5f;
		Vector3 direction = (enemyPos - startPos).Normalize();

		// Transform
		// プラズマレーザーの場合は細長くスケールを調整
		Vector3 projectileScale = { 1.0f, 1.0f, 1.0f };
		if (isBuffActive && specialType == SkillSpecialChoice::PlasmaLaser)
		{
			projectileScale = { 0.2f, 0.2f, 20.0f }; // さらに長いビーム状に
		}

		registry.AddComponent<TransformComponent>(proj, { startPos, turretTrans.localRotation_, projectileScale });

		// ProjectileComponent
		ProjectileComponent pc;
		pc.type_ = ProjectileComponent::Type::Lmb; 
		pc.speed_ = 80.0f; 
		pc.velocity_ = direction * pc.speed_;
		pc.lifetime_ = 1.5f;

		// モード分岐
		if (isBuffActive && specialType == SkillSpecialChoice::MissileSalvo)
		{
			pc.speed_ = 40.0f; // 追尾を見やすくするため
			pc.velocity_ = direction * pc.speed_;
			pc.damage_ = turret.damage_ * eSalvoDmgMult;
			pc.isHoming_ = true;
			pc.targetEntity_ = closestEnemy;
			pc.trailType_ = ProjectileComponent::TrailType::Homing;
			pc.trailId_ = HomingTrailManager::GetInstance().RegisterBulletManual();
			HomingTrailManager::GetInstance().UpdateBulletManual(pc.trailId_, startPos);
		}
		else if (isBuffActive && specialType == SkillSpecialChoice::PlasmaLaser)
		{
			pc.type_ = ProjectileComponent::Type::Beam; // 各フレームで線を引くためのフラグ
			pc.damage_ = turret.damage_ * 0.05f; // レーザーは手数が多いので大幅に低下
			pc.isHoming_ = false;
			pc.pierceCount_ = 99; // 無限貫通
			pc.speed_ = 120.0f;   // レーザーらしく速く
			pc.velocity_ = direction * pc.speed_;
			pc.trailType_ = ProjectileComponent::TrailType::Bullet;
			pc.trailId_ = BulletTrailManager::GetInstance().RegisterBulletManual();
			BulletTrailManager::GetInstance().UpdateBulletManual(pc.trailId_, startPos);
		}
		else
		{
			pc.damage_ = turret.damage_;
			pc.isHoming_ = false;
			pc.trailType_ = ProjectileComponent::TrailType::Bullet;
			pc.trailId_ = BulletTrailManager::GetInstance().RegisterBulletManual();
			BulletTrailManager::GetInstance().UpdateBulletManual(pc.trailId_, startPos);
		}

		registry.AddComponent<ProjectileComponent>(proj, pc);

		// コライダー設定
		ecs::ColliderComponent col;
		col.type_ = ColliderType::Sphere;
		col.sphere_.radius = 0.5f;
		col.isTrigger_ = true;
		col.layer = CollisionLayer::PlayerBullet;
		col.mask = CollisionLayer::Enemy | CollisionLayer::Obstacle;

		float shotDamage = pc.damage_;

		// ヒット時処理
		col.onCollisionEnter = [this, &registry, proj, shotDamage](const ecs::CollisionPartnerInfo& other) {
			if (!registry.IsAlive(other.entity)) return;
			if (!registry.HasComponent<ecs::TagComponent>(other.entity)) return;
			if (registry.GetComponent<ecs::TagComponent>(other.entity).type != ecs::TagComponent::Type::Enemy) return;

			EntityID victim = other.entity;

			// ダメージ適用
			if (registry.HasComponent<ecs::StatusComponent>(victim))
			{
				auto& status = registry.GetComponent<ecs::StatusComponent>(victim);
				status.hp_.SetBase(status.hp_.GetBase() - shotDamage);
			}

			// ヒットエフェクト
			Vector3 hitPos = registry.GetComponent<TransformComponent>(victim).localPosition_;
			ParticleManager::GetInstance()->Play("hit_effect_ver2", hitPos);

			registry.DestroyEntityDeferred(proj);
		};

		registry.AddComponent<ecs::ColliderComponent>(proj, col);
		registry.AddComponent<CollisionResponseComponent>(proj, {});

		Audio::GetInstance()->PlayWave("fire", false);
	}
}

void TurretSystem::UpdateLaserBeam(EntityID turretEntity, TurretComponent& turret, Registry& registry, float dt)
{
	(void)dt;
	auto& turretTrans = registry.GetComponent<TransformComponent>(turretEntity);

	// ターゲット検索
	EntityID closestEnemy = kInvalidEntity;
	float closestDistSq = turret.range_ * turret.range_;

	auto tagView = registry.View<ecs::TagComponent>();
	if (tagView)
	{
		for (uint32_t j = 0; j < tagView->GetSize(); j++)
		{
			if (tagView->GetDataFromDenseIndex(j).type != ecs::TagComponent::Type::Enemy) continue;
			EntityID enemy = tagView->GetEntityFromDenseIndex(j);
			if (!registry.HasComponent<TransformComponent>(enemy)) continue;
			auto& enemyTrans = registry.GetComponent<TransformComponent>(enemy);
			float distSq = (enemyTrans.localPosition_ - turretTrans.localPosition_).LengthSquared();
			if (distSq < closestDistSq)
			{
				closestDistSq = distSq;
				closestEnemy = enemy;
			}
		}
	}

	// ターゲットがいない場合はビームを削除して終了
	if (closestEnemy == kInvalidEntity)
	{
		if (turret.activeBeam_ != kInvalidEntity)
		{
			registry.DestroyEntityDeferred(turret.activeBeam_);
			turret.activeBeam_ = kInvalidEntity;
		}
		if (turret.laserEffect_)
		{
			turret.laserEffect_->Stop();
			turret.laserEffect_->SetAutoRemove(true); // あとはマネージャーの自動削除に任せる
			turret.laserEffect_ = nullptr;
		}
		return;
	}

	// ビームの向きを計算
	Vector3 targetPos = registry.GetComponent<TransformComponent>(closestEnemy).localPosition_;
	targetPos.y += 0.5f;
	Vector3 startPos = turretTrans.localPosition_;
	startPos.y += 0.5f;
	Vector3 direction = (targetPos - startPos).Normalize();

	// タレット自体をターゲットに向ける
	float targetYaw = std::atan2(direction.x, direction.z);
	turretTrans.localRotation_.y = MathUtils::LerpAngle(turretTrans.localRotation_.y, targetYaw, 0.2f);
	turretTrans.isDirty_ = true;

	// ビームEntityの生成（未生成の場合）
	if (turret.activeBeam_ == kInvalidEntity || !registry.IsAlive(turret.activeBeam_))
	{
		turret.activeBeam_ = registry.CreateEntity();
		
		// レンダリング設定（パーティクルを使うのでモデルは不要だが、Transform管理用にEntityは維持）
		// InstancedRenderComponent render; ... (削除)

		// OBBコライダー設定
		ecs::ColliderComponent col;
		col.type_ = ColliderType::OBB;
		col.obb_.size = { 0.5f, 0.5f, 0.5f }; // 半径0.5の単位OBB (スケールで調整)
		col.isTrigger_ = true;
		col.layer = CollisionLayer::PlayerBullet;
		col.mask = CollisionLayer::Enemy;

		// 継続ダメージ処理 (onCollisionStay)
		float dps = turret.damage_ * 30.0f; // 秒間ダメージ（バースト用に大幅強化）
		col.onCollisionStay = [&registry, dps](const ecs::CollisionPartnerInfo& other) {
			if (!registry.IsAlive(other.entity)) return;
			if (!registry.HasComponent<ecs::StatusComponent>(other.entity)) return;
			
			auto& status = registry.GetComponent<ecs::StatusComponent>(other.entity);
			float frameDmg = dps * TimeManager::GetInstance().GetGameContext().deltaTime;
			status.hp_.SetBase(status.hp_.GetBase() - frameDmg);

			// ヒットエフェクト（たまに出す）
			static float effectTimer = 0.0f;
			effectTimer += TimeManager::GetInstance().GetGameContext().deltaTime;
			if (effectTimer > 0.1f)
			{
				Vector3 hitPos = registry.GetComponent<TransformComponent>(other.entity).localPosition_;
				ParticleManager::GetInstance()->Play("hit_effect_ver2", hitPos);
				effectTimer = 0.0f;
			}
		};

		registry.AddComponent<ecs::ColliderComponent>(turret.activeBeam_, col);
		registry.AddComponent<CollisionResponseComponent>(turret.activeBeam_, {});
	}

	// ビームのトランスフォーム更新
	if (!registry.HasComponent<TransformComponent>(turret.activeBeam_))
	{
		registry.AddComponent<TransformComponent>(turret.activeBeam_, { (startPos + targetPos) * 0.5f, turretTrans.localRotation_, { 0.3f, 0.3f, std::sqrt(closestDistSq) } });
	}

	auto& beamTrans = registry.GetComponent<TransformComponent>(turret.activeBeam_);
	// 位置はタレットとターゲットの中点
	beamTrans.localPosition_ = (startPos + targetPos) * 0.5f;
	// 回転はターゲットの方向
	beamTrans.localRotation_ = turretTrans.localRotation_; 
	// スケールはターゲットまでの距離
	float dist = std::sqrt(closestDistSq);
	beamTrans.localScale_ = { 0.3f, 0.3f, dist }; 
	beamTrans.isDirty_ = true;

	// パーティクルエフェクトの更新
	if (!turret.laserEffect_ || !turret.laserEffect_->IsPlaying())
	{
		turret.laserEffect_ = ParticleManager::GetInstance()->Play("turret_lazer", startPos);
		if (turret.laserEffect_) {
			turret.laserEffect_->SetAutoRemove(false); // 手動でStopするまで維持
		}
	}

	if (turret.laserEffect_)
	{
		// OBBと同じ位置（タレットとターゲットの中点）に設定
		turret.laserEffect_->SetPosition(beamTrans.localPosition_);
		
		// 方向と長さを同期
		float yawDeg = targetYaw * (180.0f / 3.14159265f); // ラジアンから度へ
		
		for (uint32_t j = 0; j < turret.laserEffect_->GetEmitterCount(); j++)
		{
			auto* emitter = turret.laserEffect_->GetEmitter(j);
			if (!emitter) continue;

			// 回転の同期 (X軸=90固定, Y軸=ターゲット方向)
			if (auto* rotModule = emitter->GetModule<InitialRotationModule>())
			{
				rotModule->SetRotationRange({ 90.0f, yawDeg, 0 }, { 90.0f, yawDeg, 0 });
			}

			// スケールの同期 (Y軸 = 長さ。見た目の調整のためOBBのZスケールの半分に設定)
			if (auto* scaleModule = emitter->GetModule<InitialScaleModule>())
			{
				Vector3 minS = scaleModule->GetMinScale();
				Vector3 maxS = scaleModule->GetMaxScale();
				minS.y = beamTrans.localScale_.z * 0.5f;
				maxS.y = beamTrans.localScale_.z * 0.5f;
				scaleModule->SetScaleRange(minS, maxS);
			}
		}
	}
}
