#pragma once

#include "StatusValue.h"
#include "application/GameObject/base/GameObject.h"

class CombatableObject : public GameObject
{
public:
    virtual ~CombatableObject() = default;
    explicit CombatableObject(const std::string& tag)
        : GameObject(tag)
    {
		// 初期化しておく
        hp_.base = 100.0f;
        maxHp_.base = 100.0f;
        attackPower_.base = 10.0f;
        isAlive_ = true;
    }

    // HP
    virtual void SetHp(float v) { hp_.base = v; }
    virtual float GetHp() const { return hp_.Calc(); }
    virtual void SetMaxHp(float v) { maxHp_.base = v; }
    virtual float GetMaxHp() const { return maxHp_.Calc(); }

    // 攻撃力
    virtual void SetAttackPower(float v) { attackPower_.base = v; }
    virtual float GetAttackPower() const { return attackPower_.Calc(); }

    // 生存状態
    virtual bool IsAlive() const { return isAlive_; }
    virtual void SetAlive(bool alive) { isAlive_ = alive; }

    // ステータス更新（毎フレーム呼ぶ）
    virtual void UpdateStatus(float deltaTime)
    {
        hp_.Update(deltaTime);
        maxHp_.Update(deltaTime);
        attackPower_.Update(deltaTime);
    }

protected:
	StatusValue hp_;                // 体力
	StatusValue maxHp_;             // 最大体力
	StatusValue attackPower_;       // 攻撃力
	bool isAlive_;                  // 生存状態
};