#include "SelectorNode.h"

NodeStatus SelectorNode::Tick(Blackboard& blackboard)
{
    while (currentIndex < children.size())
    {
        NodeStatus status = children[currentIndex]->Tick(blackboard);
        if (status == NodeStatus::Success)
        {
            Reset();
            return NodeStatus::Success;
        }
        if (status == NodeStatus::Running)
        {
            return NodeStatus::Running;
        }
        // Failureなら次のノードへ
        ++currentIndex;
    }
    // 全てFailure
    Reset();
    return NodeStatus::Failure;
}
