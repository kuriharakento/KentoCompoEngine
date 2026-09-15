#include "editor/SceneGizmo.h"

#include <algorithm>

namespace KCE
{
std::unique_ptr<SceneGizmo> SceneGizmo::instance_ = nullptr;

SceneGizmo* SceneGizmo::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<SceneGizmo>();
	}
	return instance_.get();
}

bool SceneGizmo::HasInstance()
{
	return instance_ != nullptr;
}

void SceneGizmo::RegisterTarget(void* owner, SelectionKind kind, GizmoTarget target)
{
	entries_.push_back({ owner, kind, std::move(target) });
}

void SceneGizmo::Unregister(void* owner)
{
	entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [owner](const Entry& entry) { return entry.owner == owner; }), entries_.end());
}
} // namespace KCE

#ifdef USE_IMGUI

#include <cmath>

#include "ImGui/imgui.h"
#include "ImGuizmo/ImGuizmo.h"
#include "base/Camera.h"
#include "editor/SceneViewContext.h"
#include "manager/editor/DebugUIManager.h"
#include "math/MatrixFunc.h"

namespace KCE
{
namespace
{
// これより小さい拡大率は潰れたとみなす。軸の正規化で NaN を出さないため
constexpr float kMinGizmoScale = 1.0e-4f;
// ツールバーを Scene 画像の角から離す幅（ピクセル）
constexpr float kToolbarMargin = 8.0f;

/**
 * @brief 選んでいる操作が使えなければ、使える中で最初のものにする
 * @details カメラで拡大を選んでいたら移動、平行光源で移動を選んでいたら回転、のように落とす
 */
uint32_t ResolveOperation(uint32_t selected, uint32_t operations)
{
	if (operations & selected)
	{
		return selected;
	}
	for (const uint32_t candidate : { kGizmoTranslate, kGizmoRotate, kGizmoScale })
	{
		if (operations & candidate)
		{
			return candidate;
		}
	}
	return 0;
}

/**
 * @brief ワールド行列を位置・回転・拡大率に分ける
 * @return 拡大率が潰れていて分けられないなら false
 */
bool Decompose(const Matrix4x4& world, GizmoResult& out)
{
	const auto length = [](const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); };
	const Vector3 axisX = { world.m[0][0], world.m[0][1], world.m[0][2] };
	const Vector3 axisY = { world.m[1][0], world.m[1][1], world.m[1][2] };
	const Vector3 axisZ = { world.m[2][0], world.m[2][1], world.m[2][2] };

	out.translate = { world.m[3][0], world.m[3][1], world.m[3][2] };
	out.scale = { length(axisX), length(axisY), length(axisZ) };
	if (out.scale.x < kMinGizmoScale || out.scale.y < kMinGizmoScale || out.scale.z < kMinGizmoScale)
	{
		return false;
	}

	Matrix4x4 rotationMatrix = MakeIdentity4x4();
	rotationMatrix.m[0][0] = axisX.x / out.scale.x; rotationMatrix.m[0][1] = axisX.y / out.scale.x; rotationMatrix.m[0][2] = axisX.z / out.scale.x;
	rotationMatrix.m[1][0] = axisY.x / out.scale.y; rotationMatrix.m[1][1] = axisY.y / out.scale.y; rotationMatrix.m[1][2] = axisY.z / out.scale.y;
	rotationMatrix.m[2][0] = axisZ.x / out.scale.z; rotationMatrix.m[2][1] = axisZ.y / out.scale.z; rotationMatrix.m[2][2] = axisZ.z / out.scale.z;
	out.rotate = Quaternion::FromMatrix(rotationMatrix);
	return true;
}
} // namespace

void SceneGizmo::Initialize()
{
	DebugUIManager::GetInstance()->RegisterSceneOverlay(this, [this]() { Draw(); });
}

void SceneGizmo::Finalize()
{
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
	entries_.clear();
	instance_.reset();
}

const SceneGizmo::Entry* SceneGizmo::FindTarget(SelectionKind kind, const SelectionItem& item, Matrix4x4& world, uint32_t& operations) const
{
	for (const Entry& entry : entries_)
	{
		if (entry.kind != kind || !entry.target.getPose)
		{
			continue;
		}
		operations = kGizmoAll;
		if (entry.target.getPose(item, world, operations) && operations != 0)
		{
			return &entry;
		}
	}
	return nullptr;
}

void SceneGizmo::Draw()
{
	if (!SceneViewContext::HasInstance())
	{
		return;
	}
	const SceneViewRect& rect = SceneViewContext::GetInstance()->GetViewportRect();
	Camera* viewCamera = SceneViewContext::GetInstance()->GetCamera();
	if (!rect.IsValid() || !viewCamera)
	{
		return;
	}

	// 主選択の種類に登録が無ければ、None の登録（シーケンスのカメラなど）を出す
	const SelectionItem& primary = SelectionContext::GetInstance()->GetPrimary();
	Matrix4x4 world = MakeIdentity4x4();
	uint32_t operations = 0;
	const Entry* found = (primary.kind != SelectionKind::None) ? FindTarget(primary.kind, primary, world, operations) : nullptr;
	if (!found)
	{
		found = FindTarget(SelectionKind::None, primary, world, operations);
	}

	DrawToolbar(found ? operations : 0);

	if (!found)
	{
		wasUsing_ = false;
		return;
	}

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(rect.x, rect.y, rect.width, rect.height);

	// ImGuizmo は行ベクトル規約の float[16] を扱う。
	// このエンジンの Matrix4x4 と並びが一致するため、そのまま渡せる。
	const Matrix4x4 view = viewCamera->GetViewMatrix();
	const Matrix4x4 projection = viewCamera->GetProjectionMatrix();

	const uint32_t operation = ResolveOperation(operation_, operations);
	ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;
	if (operation == kGizmoRotate)
	{
		gizmoOperation = ImGuizmo::ROTATE;
	}
	else if (operation == kGizmoScale)
	{
		gizmoOperation = ImGuizmo::SCALE;
	}
	// 拡大縮小は物自身の軸に沿ってしかできないので、ワールド指定でもローカルで出す
	const ImGuizmo::MODE mode = (worldSpace_ && gizmoOperation != ImGuizmo::SCALE) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

	const bool changed = ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], gizmoOperation, mode, &world.m[0][0]);

	// 掴んだ瞬間に番号を進めて、ドラッグ1回ごとに別の Undo にする
	const bool isUsing = ImGuizmo::IsUsing();
	if (isUsing && !wasUsing_)
	{
		++dragId_;
	}
	wasUsing_ = isUsing;

	GizmoResult after;
	if (!changed || !found->target.apply || !Decompose(world, after))
	{
		return;
	}
	found->target.apply(primary, after, dragId_);
}

void SceneGizmo::DrawToolbar(uint32_t operations)
{
	const SceneViewRect& rect = SceneViewContext::GetInstance()->GetViewportRect();
	ImGui::SetCursorScreenPos(ImVec2(rect.x + kToolbarMargin, rect.y + kToolbarMargin));

	const ImGuiChildFlags childFlags = ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle;
	if (ImGui::BeginChild("##SceneGizmoToolbar", ImVec2(0.0f, 0.0f), childFlags, ImGuiWindowFlags_NoScrollbar))
	{
		// 今の対象で使えない操作は押せなくする（押しても使える操作に落ちるだけで紛らわしい）
		const auto operationButton = [&](const char* label, uint32_t operation)
		{
			ImGui::BeginDisabled(operations != 0 && (operations & operation) == 0);
			if (ImGui::RadioButton(label, ResolveOperation(operation_, operations == 0 ? kGizmoAll : operations) == operation))
			{
				operation_ = operation;
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
		};
		operationButton("移動", kGizmoTranslate);
		operationButton("回転", kGizmoRotate);
		operationButton("拡大", kGizmoScale);
		ImGui::Checkbox("ワールド", &worldSpace_);
	}
	// ツールバーの上ではマウスをゲームへ渡さない（Scene 画像の上の判定は画像を描いた時点で済んでいるので上書きする）
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
	{
		SceneViewContext::GetInstance()->SetHovered(false);
	}
	ImGui::EndChild();
}
} // namespace KCE

#else

namespace KCE
{
void SceneGizmo::Initialize() {}

void SceneGizmo::Finalize()
{
	entries_.clear();
	instance_.reset();
}
} // namespace KCE

#endif
