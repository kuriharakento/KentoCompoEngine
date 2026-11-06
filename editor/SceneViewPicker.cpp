#include "editor/SceneViewPicker.h"

namespace KCE
{
std::unique_ptr<SceneViewPicker> SceneViewPicker::instance_ = nullptr;

SceneViewPicker* SceneViewPicker::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<SceneViewPicker>();
	}
	return instance_.get();
}

bool SceneViewPicker::HasInstance()
{
	return instance_ != nullptr;
}
} // namespace KCE

#ifdef USE_IMGUI

#include <cmath>
#include <utility>

#include "ImGui/imgui.h"
#include "ImGuizmo/ImGuizmo.h"
#include "base/Camera.h"
#include "editor/SceneViewContext.h"
#include "editor/SelectionContext.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/manager/GameObjectManager.h"
#include "graphics/3d/IRenderable3d.h"
#include "manager/editor/DebugUIManager.h"
#include "math/AABB.h"
#include "math/MatrixFunc.h"

namespace KCE
{
namespace
{
// これより小さい拡大率は潰れたとみなす。逆行列が作れないので選ばない
constexpr float kMinPickScale = 1.0e-4f;
// 射影後の w がこれより小さいとき、画面上の点を戻せないとみなす
constexpr float kMinW = 1.0e-8f;
// レイの平行とみなす幅。これより小さい成分では、その軸の板との交差は「中にいるか」だけ見る
constexpr float kParallelEpsilon = 1.0e-8f;
// これより動かして離したらドラッグとみなし、クリックとして扱わない（ピクセル）
constexpr float kClickDragThreshold = 4.0f;

/**
 * @brief 行ベクトル規約で、位置に行列をかけて w で割る
 * @return w が潰れていて戻せないなら false
 */
bool TransformPoint(const Matrix4x4& matrix, float x, float y, float z, Vector3& out)
{
	const auto& m = matrix.m;
	const float w = x * m[0][3] + y * m[1][3] + z * m[2][3] + m[3][3];
	if (std::fabs(w) < kMinW)
	{
		return false;
	}
	out.x = (x * m[0][0] + y * m[1][0] + z * m[2][0] + m[3][0]) / w;
	out.y = (x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1]) / w;
	out.z = (x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2]) / w;
	return true;
}

/** @brief 行ベクトル規約で、向きに行列の回転・拡大の部分だけをかける */
Vector3 TransformDirection(const Matrix4x4& matrix, const Vector3& v)
{
	const auto& m = matrix.m;
	return {
		v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0],
		v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1],
		v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2],
	};
}

float AxisLength(const Matrix4x4& matrix, int row)
{
	const auto& m = matrix.m;
	return std::sqrt(m[row][0] * m[row][0] + m[row][1] * m[row][1] + m[row][2] * m[row][2]);
}

/**
 * @brief 直線と軸に沿った箱が当たるか
 * @param origin 直線の始点（箱と同じ座標系）
 * @param direction 直線の向き（長さはそのまま。t はこの長さを単位にする）
 * @param bounds 箱
 * @param outT 当たったときの、始点から手前の面までの t。始点が箱の中なら 0
 * @return 当たって、t が 0 から 1 の間にあれば真
 */
bool IntersectRayAabb(const Vector3& origin, const Vector3& direction, const AABB& bounds, float& outT)
{
	float tNear = 0.0f;
	float tFar = 1.0f;
	const float o[3] = { origin.x, origin.y, origin.z };
	const float d[3] = { direction.x, direction.y, direction.z };
	const float lo[3] = { bounds.min_.x, bounds.min_.y, bounds.min_.z };
	const float hi[3] = { bounds.max_.x, bounds.max_.y, bounds.max_.z };
	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::fabs(d[axis]) < kParallelEpsilon)
		{
			// この軸に平行。板の外から始まっていれば当たらない
			if (o[axis] < lo[axis] || o[axis] > hi[axis])
			{
				return false;
			}
			continue;
		}
		float t0 = (lo[axis] - o[axis]) / d[axis];
		float t1 = (hi[axis] - o[axis]) / d[axis];
		if (t0 > t1)
		{
			std::swap(t0, t1);
		}
		tNear = (t0 > tNear) ? t0 : tNear;
		tFar = (t1 < tFar) ? t1 : tFar;
		if (tNear > tFar)
		{
			return false;
		}
	}
	outT = tNear;
	return true;
}
} // namespace

void SceneViewPicker::Initialize()
{
	DebugUIManager::GetInstance()->RegisterSceneOverlay(this, [this]() { Update(); });
}

void SceneViewPicker::Finalize()
{
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->Unregister(this);
	}
	instance_.reset();
}

GameObject* SceneViewPicker::Pick(const Matrix4x4& viewProjection, float ndcX, float ndcY) const
{
	// 画面上の 1 点を、手前の面（深度 0）と奥の面（深度 1）に戻して、その間をレイにする
	const Matrix4x4 inverse = Inverse(viewProjection);
	Vector3 nearPoint;
	Vector3 farPoint;
	if (!TransformPoint(inverse, ndcX, ndcY, 0.0f, nearPoint) || !TransformPoint(inverse, ndcX, ndcY, 1.0f, farPoint))
	{
		return nullptr;
	}
	// 向きは正規化しない。物ごとのローカル座標へ移しても t の単位が変わらず、そのまま見比べられる
	const Vector3 direction = farPoint - nearPoint;

	GameObject* best = nullptr;
	float bestT = 2.0f;
	for (GameObject* object : GameObjectManager::GetInstance()->GetGameObjects())
	{
		if (!object || !object->IsActive())
		{
			continue;
		}
		IRenderable3d* renderable = object->GetRenderable3d();
		AABB localBounds;
		if (!renderable || !renderable->TryGetLocalBounds(localBounds))
		{
			continue;
		}
		const Matrix4x4 world = renderable->GetWorldMatrix();
		if (AxisLength(world, 0) < kMinPickScale || AxisLength(world, 1) < kMinPickScale || AxisLength(world, 2) < kMinPickScale)
		{
			continue;
		}
		// レイを物のローカル座標へ移して、境界箱と当てる
		const Matrix4x4 inverseWorld = Inverse(world);
		Vector3 localOrigin;
		if (!TransformPoint(inverseWorld, nearPoint.x, nearPoint.y, nearPoint.z, localOrigin))
		{
			continue;
		}
		const Vector3 localDirection = TransformDirection(inverseWorld, direction);
		float t = 0.0f;
		if (IntersectRayAabb(localOrigin, localDirection, localBounds, t) && t < bestT)
		{
			bestT = t;
			best = object;
		}
	}
	return best;
}

void SceneViewPicker::Update()
{
	if (!SceneViewContext::HasInstance())
	{
		return;
	}
	SceneViewContext* view = SceneViewContext::GetInstance();
	if (!view->IsReady())
	{
		return;
	}

	// ツールバーの上などは IsHovered が偽になっている
	const bool hovered = view->IsHovered() && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing();
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		pressedOnScene_ = hovered && !ImGui::GetIO().KeyAlt;
	}
	if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		return;
	}

	// ドラッグしていたら、カメラ操作などの途中なので選び直さない
	const ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, kClickDragThreshold);
	const bool isClick = pressedOnScene_ && hovered && dragDelta.x == 0.0f && dragDelta.y == 0.0f;
	pressedOnScene_ = false;
	if (!isClick)
	{
		return;
	}

	const SceneViewRect& rect = view->GetViewportRect();
	const ImVec2 mouse = ImGui::GetMousePos();
	const float ndcX = (mouse.x - rect.x) / rect.width * 2.0f - 1.0f;
	const float ndcY = 1.0f - (mouse.y - rect.y) / rect.height * 2.0f;

	const Camera* camera = view->GetCamera();
	const Matrix4x4 viewProjection = camera->GetViewMatrix() * camera->GetProjectionMatrix();
	// 当たった物を選び、何にも当たらなければ選択を解除する
	SelectionContext::GetInstance()->SelectGameObject(Pick(viewProjection, ndcX, ndcY));
}
} // namespace KCE

#else

namespace KCE
{
void SceneViewPicker::Initialize() {}

void SceneViewPicker::Finalize()
{
	instance_.reset();
}
} // namespace KCE

#endif
