#pragma once

namespace GameObjectTag
{
	// 共通タグ
	namespace Common {
		const std::string GameObject = "GameObject";
		const std::string Character = "Character";
		const std::string CombatableObject = "CombatableObject";
		const std::string EnemyBase = "EnemyBase";
	}

	// キャラクター
	namespace Character {
		// プレイヤー
		const std::string Player = "Player";
		const std::string PlayerRightArm = "PlayerRightArm";

		// 敵キャラクター
		const std::string AssaultEnemy = "AssaultEnemy";
		const std::string PistolEnemy = "PistolEnemy";
		const std::string ShotgunEnemy = "ShotgunEnemy";

		const std::string KnifeEnemy = "KnifeEnemy";
		const std::string KnifeEnemyRightArm = "KnifeEnemyRightArm";
		const std::string KnifeEnemyLeftArm = "KnifeEnemyLeftArm";
	}

	// 武器
	namespace Weapon {
		const std::string PlayerBullet = "PlayerBullet";
		const std::string EnemyBullet = "EnemyBullet";
		const std::string Knife = "Knife";
	}

	// アイテム
	namespace Item {
		const std::string Obstacle = "Obstacle";
		const std::string BarrierBlock = "BarrierBlock";
	}
}