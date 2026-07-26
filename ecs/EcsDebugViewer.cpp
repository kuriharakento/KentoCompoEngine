#include "EcsDebugViewer.h"
#include "imgui/imgui.h"

namespace KCE
{
// No namespaces

void EcsDebugViewer::Initialize()
{
    m_showWindow = true;
}

void EcsDebugViewer::DrawImGui(const Registry& registry)
{
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Registry Status", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uint32_t active = registry.GetActiveEntityCount();
        uint32_t maxEnt = registry.GetMaxEntityCount();
        
        ImGui::Text("Active Entities: %u / %u", active, maxEnt);
        
        // メモリ使用率のプログレスバー表示
        float fraction = (maxEnt > 0) ? static_cast<float>(active) / static_cast<float>(maxEnt) : 0.0f;
        char buf[32];
        sprintf_s(buf, "%d/%d", active, maxEnt);
        ImGui::ProgressBar(fraction, ImVec2(0.f, 0.f), buf);
    }
#endif
}
} // namespace KCE
