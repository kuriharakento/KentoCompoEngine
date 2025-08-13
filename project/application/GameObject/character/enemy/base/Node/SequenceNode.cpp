#include "SequenceNode.h"

NodeStatus SequenceNode::Tick(Blackboard& blackboard)
{
    while (currentIndex < children.size())
    {
        NodeStatus status = children[currentIndex]->Tick(blackboard);
        if (status == NodeStatus::Failure)
        {
            Reset();
            return NodeStatus::Failure;
        }
        if (status == NodeStatus::Running)
        {
            return NodeStatus::Running;
        }
        // Successなら次のノードへ
        ++currentIndex;
    }
    // 全てSuccess
    Reset();
    return NodeStatus::Success;
}
