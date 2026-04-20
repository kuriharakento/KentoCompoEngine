#pragma once

#include <vector>
#include <memory>
#include "math/Vector3.h"
#include "application/gameObject/combatable/character/enemy/base/Node/BehaviorTree/BehaviorTree.h"

/**
 * @brief 敵のAIと振る舞いを管理するためのECSコンポーネント
 *
 * 従来の AssaultEnemyBehavior が持っていたステート変数や設定値、
 * パトロール座標群、および BehaviorTree のインスタンスを保持します。
 * (メモリ再配置時のダングリングポインタを防ぐため、BTのラムダ式内で this キャプチャは禁止されます)
 */
struct EnemyAIComponent
{
	// ターゲットのEntityID (kInvalidEntity などを無効値として扱う)
	uint32_t targetEntity_ = 0xFFFFFFFF;

	// AIパラメータ
	float maxMoveDistancePerFrame_ = 0.3f;
	float attackRange_ = 18.0f;
	float minRange_ = 10.0f;
	float maxRange_ = 25.0f;
	float extendedMinRange_ = 8.0f;
	float extendedMaxRange_ = 25.0f;
	float detectionRange_ = 35.0f;

	// 横移動パラメータ
	Vector3 strafeDirection_;
	float strafeChangeInterval_ = 1.5f;
	float strafeTendencyFactor_ = 0.5f;
	float strafeTimer_ = 0.0f;
	bool isStrafing_ = false;
	float strafeDuration_ = 3.0f;
	float strafeProbability_ = 0.25f;

	// パトロールパラメータ
	std::vector<Vector3> patrolPoints_;
	int currentPatrolIndex_ = 0;
	float patrolRadius_ = 20.0f;
	bool patrolInitialized_ = false;
	float patrolSpeed_ = 0.6f;

	// 戦鬥・行動タイマー
	float stateTimer_ = 0.0f;
	float actionCooldown_ = 0.0f;
	float positionCheckTimer_ = 0.0f;
	float spawnTimer_ = 0.0f;
	float combatStateTimer_ = 0.0f;

	// スタック(引っかかり)検出パラメータ
	Vector3 lastPosition_;
	float stuckTimer_ = 0.0f;
	float stuckThreshold_ = 1.0f;
	bool potentiallyStuck_ = false;

	// 回り込み (Flanking) の状態
	bool isFlanking_ = false;
	float flankTimer_ = 0.0f;
	float flankDirectionSign_ = 1.0f;

	// ビヘイビアツリー (std::unique_ptr)
	// ComponentArrayでのメモリ移動に対応するため、独自のコピーはせずムーブセマンティクスに頼る
	std::unique_ptr<BehaviorTree> behaviorTree_;

	// ムーブコンストラクタ (std::unique_ptr が含まれるため必要)
	EnemyAIComponent() = default;
	EnemyAIComponent(EnemyAIComponent&&) noexcept = default;
	EnemyAIComponent& operator=(EnemyAIComponent&&) noexcept = default;
};
