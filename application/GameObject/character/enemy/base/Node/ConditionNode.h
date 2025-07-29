#pragma once
#include "BTNode.h"
#include <functional>


/**
 * \brief 条件チェックノード
 */
class ConditionNode : public BTNode
{
public:
    using ConditionFunction = std::function<bool()>;

    ConditionNode(ConditionFunction func) : condition(func) {}

    NodeStatus Tick() override
    {
        return condition() ? NodeStatus::Success : NodeStatus::Failure;
    }

    void Reset() override
    {
        // 条件ノードも状態は持たないので特になし
    }

private:
    ConditionFunction condition;

};
