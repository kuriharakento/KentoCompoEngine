#pragma once
#include "BlackBoard.h"

// ノードのステータスを表す
enum class NodeStatus { Success, Failure, Running };

/**
 * \brief すべてのノードの共通インターフェース
 */
class BTNode
{
public:
    virtual ~BTNode() = default;
	virtual NodeStatus Tick(Blackboard& blackboard) = 0; // ノードの実行
    virtual void Reset() {} // 状態のリセットなどに使用
};