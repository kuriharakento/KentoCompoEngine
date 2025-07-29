#pragma once
#include <memory>

#include "../BTNode.h"

/**
 * \brief BT全体の実行と管理を行う
 */
class BehaviorTree
{
private:
    std::unique_ptr<BTNode> root;

public:
    BehaviorTree() = default;

    explicit BehaviorTree(std::unique_ptr<BTNode> rootNode)
        : root(std::move(rootNode))
    {
    }

    void SetRoot(std::unique_ptr<BTNode> rootNode)
    {
        root = std::move(rootNode);
    }

    void Tick()
    {
        if (root)
        {
            root->Tick();
        }
    }

    void Reset()
    {
        if (root)
        {
            root->Reset();
        }
    }
};