#include "gameobject/manager/GameObjectRenderer.h"

#include <algorithm>

#include "gameobject/base/GameObject.h"
#include "gameobject/component/base/IRenderableComponent.h"
#include "graphics/3d/IRenderable3d.h"
#include "graphics/3d/Object3d.h"
#include "math/Frustum.h"
#include "time/TimeManager.h"

namespace KCE
{
namespace
{
constexpr uint32_t kAabbCornerCount = 8;

/** @brief ローカルの境界箱を行列で送り、それを囲むワールドの境界箱を作る（行ベクトル） */
AABB TransformBounds(const AABB& local, const Matrix4x4& world)
{
	AABB result;
	for (uint32_t i = 0; i < kAabbCornerCount; ++i)
	{
		const float x = (i & 1) ? local.max_.x : local.min_.x;
		const float y = (i & 2) ? local.max_.y : local.min_.y;
		const float z = (i & 4) ? local.max_.z : local.min_.z;
		const Vector3 p{
			x * world.m[0][0] + y * world.m[1][0] + z * world.m[2][0] + world.m[3][0],
			x * world.m[0][1] + y * world.m[1][1] + z * world.m[2][1] + world.m[3][1],
			x * world.m[0][2] + y * world.m[1][2] + z * world.m[2][2] + world.m[3][2],
		};
		if (i == 0)
		{
			result = AABB(p, p);
			continue;
		}
		result.min_ = Vector3{ (std::min)(result.min_.x, p.x), (std::min)(result.min_.y, p.y), (std::min)(result.min_.z, p.z) };
		result.max_ = Vector3{ (std::max)(result.max_.x, p.x), (std::max)(result.max_.y, p.y), (std::max)(result.max_.z, p.z) };
	}
	return result;
}

bool MatchesPass(const GameObjectRenderer::Entry& entry, GameObjectRenderer::Pass pass)
{
	switch (pass)
	{
	case GameObjectRenderer::Pass::GBuffer:
		return entry.queue == RenderQueue::Opaque && entry.renderingType == RenderingType::Deferred;
	case GameObjectRenderer::Pass::Forward:
		return entry.queue == RenderQueue::Opaque && entry.renderingType != RenderingType::Deferred;
	case GameObjectRenderer::Pass::Transparent:
		return entry.queue == RenderQueue::Transparent;
	case GameObjectRenderer::Pass::Shadow:
		return entry.castShadow;
	}
	return false;
}
} // namespace

void GameObjectRenderer::AddEntry(GameObject* object, IRenderable3d* renderable, bool castShadow)
{
	Entry entry;
	entry.object = object;
	entry.renderable = renderable;
	AABB localBounds;
	entry.hasBounds = renderable->TryGetLocalBounds(localBounds);
	if (entry.hasBounds)
	{
		entry.worldBounds = TransformBounds(localBounds, renderable->GetWorldMatrix());
	}
	entry.layer = object->GetRenderLayer();
	entry.queue = renderable->GetRenderQueue();
	entry.renderingType = renderable->GetRenderingType();
	entry.castShadow = castShadow;
	entries_.push_back(entry);
}

void GameObjectRenderer::Collect(const std::vector<GameObject*>& roots)
{
	entries_.clear();
	collectedFrame_ = TimeManager::GetInstance().GetFrameCount();

	// 子もたどる。非アクティブな物の子は描かない（IsActive が親を見るので、木ごと飛ばしてよい）
	const auto collect = [this](const auto& self, GameObject* object) -> void
	{
		if (!object->IsActive())
		{
			return;
		}
		// 行列はここでフレームに1回だけ確定させる。子は中で親を先に確定させる
		object->EnsureRenderTransform();
		if (IRenderable3d* renderable = object->GetRenderable3d())
		{
			bool castShadow = true;
			if (const Object3d* object3d = object->GetObject3d())
			{
				castShadow = object3d->GetCastShadow();
			}
			AddEntry(object, renderable, castShadow);
		}
		// 描画物を持つコンポーネント（ModelRendererComponent など）も同じ一覧に入れる
		for (const GameObjectComponent::IRenderableComponent* renderer : object->GetRenderableComponents())
		{
			if (IRenderable3d* renderable = renderer->GetRenderable3d())
			{
				AddEntry(object, renderable, renderer->GetCastShadow());
			}
		}
		for (const auto& [name, child] : object->GetChildren())
		{
			if (child)
			{
				self(self, child.get());
			}
		}
	};
	for (GameObject* root : roots)
	{
		collect(collect, root);
	}
}

void GameObjectRenderer::EnsureCollected(const std::vector<GameObject*>& roots)
{
	if (collectedFrame_ != TimeManager::GetInstance().GetFrameCount())
	{
		Collect(roots);
	}
}

const std::vector<const GameObjectRenderer::Entry*>& GameObjectRenderer::GatherVisible(Pass pass, const Matrix4x4* viewProjection, RenderLayerMask layerMask)
{
	RollCounters();
	visible_.clear();
	// 面はビューごとに1回だけ作る
	const bool cull = cullingEnabled_ && viewProjection;
	Frustum frustum{};
	if (cull)
	{
		frustum = Frustum::FromViewProjection(*viewProjection);
	}
	for (const Entry& entry : entries_)
	{
		if (!MatchesPass(entry, pass))
		{
			continue;
		}
		// レイヤーは今まで通りカリングより先に効かせる。影はビューのレイヤーを見ない
		if (pass != Pass::Shadow && !IsVisibleInLayerMask(entry.layer, layerMask))
		{
			continue;
		}
		if (cull && entry.hasBounds && !frustum.Intersects(entry.worldBounds))
		{
			++culledCount_;
			continue;
		}
		++drawnCount_;
		visible_.push_back(&entry);
	}
	return visible_;
}

void GameObjectRenderer::RollCounters()
{
	// Update を呼ばないシーンもあるので、フレーム番号で締める
	const uint64_t frame = TimeManager::GetInstance().GetFrameCount();
	if (frame == countedFrame_)
	{
		return;
	}
	lastFrameDrawnCount_ = drawnCount_;
	lastFrameCulledCount_ = culledCount_;
	drawnCount_ = 0;
	culledCount_ = 0;
	countedFrame_ = frame;
}
} // namespace KCE
