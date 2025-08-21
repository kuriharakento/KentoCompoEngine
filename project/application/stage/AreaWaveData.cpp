#include "AreaWaveData.h"

AreaWaveData::AreaWaveData()
{
	Register("areas", &areas);
}

void AreaWaveData::DrawImGui()
{
#ifdef _DEBUG
    for (auto& area : areas)
    {
        ImGui::PushID(area.areaIndex);
        if (ImGui::CollapsingHeader(("Area " + std::to_string(area.areaIndex)).c_str()))
        {
            int waveIdx = 0;
            for (auto& wave : area.waves)
            {
                ImGui::PushID(waveIdx);
                if (ImGui::TreeNode(("Wave " + std::to_string(waveIdx)).c_str()))
                {
                    int enemyIdx = 0;
                    for (auto& enemy : wave.enemies)
                    {
                        ImGui::PushID(enemyIdx);
                        ImGui::Text("Enemy Name: %s, File: %s", enemy.name.c_str(), enemy.fileName.c_str());
                        ImGui::PopID();
                        enemyIdx++;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
                waveIdx++;
            }
        }
        ImGui::PopID();
    }
#endif
}
