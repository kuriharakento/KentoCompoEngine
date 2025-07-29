#pragma once
#include "BTNode.h"
#include <functional>

/**
 * \brief 敵の移動、攻撃、その他の具体的なアクションを実行するノード
 */
class ActionNode : public BTNode
{
public:
    using ActionFunction = std::function<NodeStatus()>;

	ActionNode(ActionFunction func) : action(func) {}

    NodeStatus Tick() override
    {
        return action();
    }

    void Reset() override { /* アクションには状態を保持しないので特になし */ }

private:
    ActionFunction action;

};
