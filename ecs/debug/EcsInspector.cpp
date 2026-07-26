#include "EcsInspector.h"
#include "imgui/imgui.h"
#include "../../../engine/ecs/components/TransformComponent.h"
#include "../../../engine/ecs/components/InstancedRenderComponent.h"
#include "../../../engine/ecs/components/LifetimeComponent.h"

namespace KCE
{
using namespace ecs;

void EcsInspector::Initialize()
{
    showWindow_ = true;
    selectedEntity_ = 0xFFFFFFFF;
}

void EcsInspector::DrawImGui(Registry& registry)
{
#ifdef USE_IMGUI
    // 1. レジストリ全体の統計情報
    if (ImGui::CollapsingHeader("Registry Statistics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uint32_t active = registry.GetActiveEntityCount();
        uint32_t maxEnt = registry.GetMaxEntityCount();
        ImGui::Text("Active Entities: %u / %u", active, maxEnt);
        float progress = (maxEnt > 0) ? (float)active / maxEnt : 0.0f;
        ImGui::ProgressBar(progress, ImVec2(-1, 0));
    }

    ImGui::Separator();

    // 2. エンティティリストと詳細表示の2ペイン
    ImGui::Columns(2, "InspectorColumns");

    // 左ペイン: Entity リスト
    DrawEntityList(registry);

    ImGui::NextColumn();

    // 右ペイン: Component エディタ
    if (selectedEntity_ != 0xFFFFFFFF && registry.IsAlive(selectedEntity_))
    {
        DrawComponentEditor(registry, selectedEntity_);
    }
    else
    {
        ImGui::Text("Select an entity to inspect.");
    }

    ImGui::Columns(1);
#endif
}

void EcsInspector::DrawEntityList(Registry& registry)
{
    ImGui::Text("Entity List");
    ImGui::InputText("Filter", searchFilter_, sizeof(searchFilter_));

    ImGui::BeginChild("EntityScrollList", ImVec2(0, 0), true);

    // 簡易的に生?EEntity を探してリストアチE?E
    // TODO: Registry に正確な全生?EEntity 取征EAPI を追加するのが望ましい
    uint32_t maxEnt = registry.GetMaxEntityCount();
    uint32_t count = 0;
    for (uint32_t i = 0; i < maxEnt && count < 500; ++i)
    {
        // 今回の Registry 実裁E??は index と世代が一致すれば Alive
        // チE??チE??表示用として、index=i で何らか?E EntityID ぁEAlive か?E走査?E?重ぁE?Eで上限付き?E?E
        // ※実際には 0x000FFFFF マスクで index を抽出してぁE??ため
        // Registry 側に全生?EEntity を返すイチE??ータがあると良ぁE
    }

    ImGui::Text("(Full iteration pending registry API optimization)");

    ImGui::EndChild();
}

void EcsInspector::DrawComponentEditor(Registry& registry, EntityID entity)
{
    ImGui::Text("ID: 0x%08X", entity);
    ImGui::Separator();

    // TransformComponent
    if (registry.HasComponent<TransformComponent>(entity))
    {
        if (ImGui::CollapsingHeader("TransformComponent", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& t = registry.GetComponent<TransformComponent>(entity);
            if (ImGui::DragFloat3("Position", &t.localPosition_.x, 0.1f))
            {
                t.isDirty_ = true;
            }
            if (ImGui::DragFloat3("Rotation", &t.localRotation_.x, 0.01f))
            {
                t.isDirty_ = true;
            }
            if (ImGui::DragFloat3("Scale", &t.localScale_.x, 0.1f))
            {
                t.isDirty_ = true;
            }
        }
    }

    // InstancedRenderComponent
    if (registry.HasComponent<InstancedRenderComponent>(entity))
    {
        if (ImGui::CollapsingHeader("InstancedRenderComponent", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& r = registry.GetComponent<InstancedRenderComponent>(entity);
            char buf[64];
            strncpy_s(buf, r.modelName_.c_str(), sizeof(buf));
            if (ImGui::InputText("Model Name", buf, sizeof(buf)))
            {
                r.modelName_ = buf;
            }
            ImGui::Checkbox("Visible", &r.isVisible_);
            ImGui::Checkbox("Use Instancing", &r.useInstancing_);
        }
    }

    // LifetimeComponent
    if (registry.HasComponent<LifetimeComponent>(entity))
    {
        if (ImGui::CollapsingHeader("LifetimeComponent", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& l = registry.GetComponent<LifetimeComponent>(entity);
            ImGui::SliderFloat("Age", &l.currentAge_, 0.0f, l.maxLifetime_);
            ImGui::DragFloat("Max Lifetime", &l.maxLifetime_, 0.1f, 0.0f, 100.0f);
        }
    }
}
} // namespace KCE
