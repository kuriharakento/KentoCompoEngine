#pragma once

#include "StatusSystem.h"
#include "application/GameObject/base/GameObject.h"
#include "application/GameObject/component/action/StatusComponent.h"

// 攻撃を受けることができるオブジェクトの基底クラス
class CombatableObject : public GameObject
{
public:
    virtual ~CombatableObject() = default;
    explicit CombatableObject(const std::string& tag = GameObjectTag::Common::CombatableObject)
        : GameObject(tag)
    {
		// ステータスコンポーネントを追加
		AddComponent("StatusComponent", std::make_unique<StatusComponent>());
    }

    // ダメージを受ける
	virtual void TakeDamage(float damage)
	{
		auto status = GetComponent<StatusComponent>();
		if (status && status->isAlive)
		{
			float newHp = status->hp.GetValue() - damage;
			status->hp.SetBase(newHp);
			if (newHp <= 0.0f)
			{
				status->isAlive = false;
				status->hp.SetBase(0.0f);
			}
		}
	}

	//======================================
	// ステータスのGetter/Setter
	//======================================

    // HP
    float GetHp() const
    {
        auto status = GetComponent<StatusComponent>();
        return status ? status->hp.GetValue() : 0.0f;
    }
    void SetHp(float v)
    {
        auto status = GetComponent<StatusComponent>();
        if (status)
        {
            status->hp.SetBase(v);
        }
    }

    // 攻撃力
    float GetAttackPower() const
    {
        auto status = GetComponent<StatusComponent>();
        return status ? status->attackPower.GetValue() : 0.0f;
    }
    void SetAttackPower(float v)
    {
        auto status = GetComponent<StatusComponent>();
        if (status)
        {
            status->attackPower.SetBase(v);
        }
    }

    // 生存状態
    bool IsAlive() const
    {
        auto status = GetComponent<StatusComponent>();
        return status ? status->isAlive : false;
    }
    void SetAlive(bool alive)
    {
        auto status = GetComponent<StatusComponent>();
        if (status)
        {
            status->isAlive = alive;
        }
    }
};
