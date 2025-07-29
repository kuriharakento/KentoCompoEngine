#include "SelectorNode.h"

NodeStatus SelectorNode::Tick()
{
    while (currentIndex < children.size())
    {
        NodeStatus status = children[currentIndex]->Tick();
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
