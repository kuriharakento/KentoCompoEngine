#include "ActionNode.h"

NodeStatus ActionNode::Tick(Blackboard& blackboard)
{
	return action(blackboard);
}
