#pragma once
#include "application/gameObject/combatable/base/StatusSystem.h"
#include "application/gameObject/component/base/IActionComponent.h"

/**
 * @brief ゲームオブジェクトのステータスを管理するコンポーネント
 *
 * HP、攻撃力、移動速度などのステータス値を管理し、更新処理を行う
 */
class StatusComponent : public IActionComponent
{
public:
	/**
	 * @brief フレームごとの更新処理
	 * @param owner このコンポーネントを所有するゲームオブジェクト
	 */
	void Update(GameObject* owner) override;

	// 現在のHP
	StatusValue hp{ kDefaultHp };
	// 最大HP
    StatusValue maxHp{ kDefaultMaxHp };
	// 攻撃力
    StatusValue attackPower{ kDefaultAttackPower };
	// 移動速度
	StatusValue moveSpeed{ kDefaultMoveSpeed };
	// 生存フラグ
	bool isAlive = true;

private:
	// デフォルトHP
	static constexpr float kDefaultHp = 100.0f;
	// デフォルト最大HP
	static constexpr float kDefaultMaxHp = 100.0f;
	// デフォルト攻撃力
	static constexpr float kDefaultAttackPower = 10.0f;
	// デフォルト移動速度
	static constexpr float kDefaultMoveSpeed = 9.0f;
	// 死亡判定のHP閾値
	static constexpr float kDeathThreshold = 0.0f;
};