#pragma once
#include <cstdint>
#include "engine/ecs/Entity.h"

/**
 * @brief タレットの状態を管理するコンポーネント。
 *
 * - タレットEntityに付与して、自動射撃ロジックで使う。
 * - owner_ は所有プレイヤー。プレイヤーが先に死なない前提で非所有ポインタ。
 */
struct TurretComponent
{
	// 所有プレイヤー（所有しない。プレイヤーのほうが長寿命の前提）
	EntityID owner_ = kInvalidEntity;

	// 攻撃間隔（秒）
	float fireInterval_ = 0.2f;
	float fireTimer_ = 0.0f;

	// 攻撃力
	float damage_ = 80.0f;

	// 攻撃射程
	float range_ = 30.0f;

	// チャネル方式のビームEntity（レーザー用）
	EntityID activeBeam_ = kInvalidEntity;

	// 再生中のレーザーエフェクト
	class ParticleEffect* laserEffect_ = nullptr;
};
