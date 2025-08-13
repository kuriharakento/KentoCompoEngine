#include "BehaviorTree.h"

void BehaviorTree::SetRoot(std::unique_ptr<BTNode> rootNode)
{
	root = std::move(rootNode);
}

void BehaviorTree::Tick()
{
    if (root)
    {
        root->Tick(blackboard);
    }
}

void BehaviorTree::Reset()
{
    if (root)
    {
        root->Reset();
    }
}
