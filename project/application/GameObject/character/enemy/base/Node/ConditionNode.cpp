#include "ConditionNode.h"

NodeStatus ConditionNode::Tick(Blackboard& blackboard)
{
	return condition(blackboard) ? NodeStatus::Success : NodeStatus::Failure;
}
