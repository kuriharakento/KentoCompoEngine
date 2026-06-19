#include "SplineData.h"
#include <fstream>

#include "base/Logger.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

SplineData::SplineData()
{
    // JSONシリアライズ用に制御点配列を登録
	Register("controlPoints",&controlPoints);
}

void SplineData::Initialize(const std::string& name)
{
    // JSONファイルから制御点を読み込み
	LoadJson(name);
}

void SplineData::DrawImGui()
{
#ifdef USE_IMGUI
	// 制御点追加ボタン
	ImGui::SameLine();
	if (ImGui::Button("Add ControlPoints"))
	{
		controlPoints.push_back(Vector3(0.0f, 0.0f, 0.0f));
	}
#endif // USE_IMGUI
	// 基底クラスの情報表示
	JsonEditableBase::DrawImGui();
}
