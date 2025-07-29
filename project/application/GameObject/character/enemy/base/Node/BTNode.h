#pragma once

// ノードのステータスを表す
enum class NodeStatus { Success, Failure, Running };

/**
 * \brief すべてのノードの共通インターフェース
 */
class BTNode
{
public:
    virtual ~BTNode() = default;
    virtual NodeStatus Tick() = 0;
    virtual void Reset() {} // 状態のリセットなどに使用
};