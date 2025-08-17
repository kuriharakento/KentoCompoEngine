#pragma once
#include <string>

#include "BTNode.h"
#include "CompositeNode.h"
#include "imgui/imgui.h"

namespace NodeUtils
{
	// NodeStatusを文字列に変換
	const std::string NodeStatusToString(NodeStatus status)
	{
		switch (status)
		{
		case NodeStatus::Success:
			return "Success";
		case NodeStatus::Failure:
			return "Failure";
		case NodeStatus::Running:
			return "Running";
		default:
			return "Unknown";
		}
	}

    // 再帰的にノードを描画
    void DrawBTNodeImGui(const BTNode* node)
    {
        if (!node) return;

        // ノード名 + アドレスによるユニークラベル
        std::string label = node->GetNodeName() + "##" + std::to_string(reinterpret_cast<uintptr_t>(node));

        ImVec4 color;
        switch (node->GetLastStatus())
        {
        case NodeStatus::Running: color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); break;
        case NodeStatus::Success: color = ImVec4(0.2f, 0.6f, 1.0f, 1.0f); break;
        case NodeStatus::Failure: color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break;
        default: color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        // TreeNodeの第1引数はユニークラベル
        bool open = ImGui::TreeNode(label.c_str(), "%s [%s]", node->GetNodeName().c_str(), NodeStatusToString(node->GetLastStatus()).c_str());

        ImGui::PopStyleColor();

        if (open)
        {
            if (auto comp = dynamic_cast<const CompositeNode*>(node))
            {
                for (const auto& child : comp->GetChildren())
                {
                    DrawBTNodeImGui(child.get());
                }
            }
            ImGui::TreePop();
        }
    }
}
