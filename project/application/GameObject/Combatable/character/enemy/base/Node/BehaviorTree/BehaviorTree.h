#pragma once
#include <memory>

#include "../BTNode.h"

/**
 * \brief BT全体の実行と管理を行う
 */
class BehaviorTree
{
public:
    BehaviorTree() = default;

    explicit BehaviorTree(std::unique_ptr<BTNode> rootNode)
        : root(std::move(rootNode))
    {
    }

    void SetRoot(std::unique_ptr<BTNode> rootNode);

    void Tick();

    void Reset();

	Blackboard& GetBlackboard() { return blackboard; }

private:
    std::unique_ptr<BTNode> root;
    Blackboard blackboard; // ノード間で共有する情報を格納する
};