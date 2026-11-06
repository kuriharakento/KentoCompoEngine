#include "gameobject/manager/GameObjectInstancing.h"

#include <algorithm>
#include <cstring>

#include "gameobject/base/GameObject.h"
#include "graphics/3d/InstancedModelRenderer.h"
#include "graphics/3d/Object3d.h"
#include "manager/graphics/ModelManager.h"
#include "time/TimeManager.h"

namespace KCE
{
namespace
{
// 1つのまとまりで描ける上限。あふれた分は1体ずつ描く
constexpr uint32_t kMaxInstancesPerGroup = 4096;
// これ未満ならまとめても得しないので1体ずつ描く
constexpr size_t kMinInstancesPerGroup = 2;
// バッファを作るときの最小の大きさ。描画ごとに別のバッファを持つので、小さく始めて足りなければ作り直す
constexpr uint32_t kMinRendererCapacity = 64;

/** @brief count 以上の最小の2の冪（1 以上）。バッファの大きさをそろえて、作り直しを減らす */
uint32_t RoundUpToPowerOfTwo(uint32_t count)
{
	uint32_t result = 1;
	while (result < count)
	{
		result <<= 1;
	}
	return result;
}

/** @brief 2つのモデルの素材が全部同じか。1体ごとに色などをいじった物をまとめないための確認 */
bool HasSameMaterials(const Model& lhs, const Model& rhs)
{
	const auto& lhsMeshes = lhs.GetMeshResources();
	const auto& rhsMeshes = rhs.GetMeshResources();
	if (lhsMeshes.size() != rhsMeshes.size())
	{
		return false;
	}
	for (size_t i = 0; i < lhsMeshes.size(); ++i)
	{
		const Material* a = lhsMeshes[i].gpuMaterial;
		const Material* b = rhsMeshes[i].gpuMaterial;
		if (!a || !b || std::memcmp(a, b, sizeof(Material)) != 0)
		{
			return false;
		}
		if (lhsMeshes[i].textureIndex != rhsMeshes[i].textureIndex)
		{
			return false;
		}
	}
	return true;
}
} // namespace

GameObjectInstancing::GameObjectInstancing() = default;
GameObjectInstancing::~GameObjectInstancing() = default;

void GameObjectInstancing::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
}

void GameObjectInstancing::Finalize()
{
	groups_.clear();
	retiredRenderers_.clear();
	activeGroups_.clear();
	singles_.clear();
}

Model* GameObjectInstancing::FindSharedModel(const Object3d* object3d)
{
	const std::string& modelName = object3d->GetModelName();
	if (modelName.empty())
	{
		return nullptr;
	}
	Model* shared = ModelManager::GetInstance()->FindModel(modelName);
	const Model* own = object3d->GetModel();
	if (!shared || !own)
	{
		return nullptr;
	}
	// 1体ごとに素材をいじっていたら、まとめると見た目が変わってしまう
	if (!HasSameMaterials(*own, *shared))
	{
		return nullptr;
	}
	return shared;
}

void GameObjectInstancing::Build(const std::vector<const GameObjectRenderer::Entry*>& visible, Camera* camera)
{
	if (!camera)
	{
		// ビューの行列が作れないなら、まとめずに今まで通り1体ずつ描く
		singles_.clear();
		activeGroups_.clear();
		lastInstancedCount_ = 0;
		lastGroupCount_ = 0;
		singles_.insert(singles_.end(), visible.begin(), visible.end());
		return;
	}
	const Matrix4x4 viewProjection = camera->GetViewProjectionMatrix();
	BuildInternal(visible, &viewProjection);
}

void GameObjectInstancing::BuildForShadow(const std::vector<const GameObjectRenderer::Entry*>& visible)
{
	// 影のシェーダーが使うのはワールド行列とライトの行列だけなので、WVP は作らない
	BuildInternal(visible, nullptr);
}

void GameObjectInstancing::BuildInternal(const std::vector<const GameObjectRenderer::Entry*>& visible, const Matrix4x4* viewProjection)
{
	singles_.clear();
	activeGroups_.clear();
	lastInstancedCount_ = 0;
	lastGroupCount_ = 0;

	if (!enabled_ || !dxCommon_ || !srvManager_)
	{
		singles_.insert(singles_.end(), visible.begin(), visible.end());
		return;
	}

	for (auto& [model, group] : groups_)
	{
		group.entries.clear();
		group.instanceData.clear();
	}

	// まとめ先は Collect のときに決めてある。ここは振り分けるだけ
	for (const GameObjectRenderer::Entry* entry : visible)
	{
		if (!entry->instancedModel)
		{
			singles_.push_back(entry);
			continue;
		}
		if (!shadowMapManagerFromObjects_)
		{
			shadowMapManagerFromObjects_ = entry->object3d->GetShadowMapManager();
		}
		Group& group = groups_[entry->instancedModel];
		group.model = entry->instancedModel;
		group.entries.push_back(entry);
	}

	// まとまりが小さいとまとめても得しないので、そのぶんは1体ずつ描く
	for (auto& [model, group] : groups_)
	{
		if (group.entries.size() < kMinInstancesPerGroup)
		{
			singles_.insert(singles_.end(), group.entries.begin(), group.entries.end());
			continue;
		}
		// ワールドと逆転置は EnsureRenderTransform で確定済みなので使い回す。ここで作るのはビューの WVP だけ
		for (const GameObjectRenderer::Entry* entry : group.entries)
		{
			const TransformationMatrix* source = entry->object3d->GetTransformationMatrixData();
			if (!source || group.instanceData.size() >= kMaxInstancesPerGroup)
			{
				// あふれた分と、行列がまだ無い物は1体ずつ描く
				singles_.push_back(entry);
				continue;
			}
			TransformationMatrix data;
			data.World = source->World;
			data.WorldInverseTranspose = source->WorldInverseTranspose;
			data.WVP = viewProjection ? (source->World * *viewProjection) : source->World;
			group.instanceData.push_back(data);
		}
		activeGroups_.push_back(&group);
		lastInstancedCount_ += static_cast<uint32_t>(group.instanceData.size());
	}
	lastGroupCount_ = static_cast<uint32_t>(activeGroups_.size());
}

InstancedModelRenderer* GameObjectInstancing::AcquireRenderer(Group& group)
{
	// フレームが変わったら、全部のまとまりのバッファを最初から使い直す
	const uint64_t frame = TimeManager::GetInstance().GetFrameCount();
	if (frame != renderersResetFrame_)
	{
		renderersResetFrame_ = frame;
		for (auto& [model, other] : groups_)
		{
			other.usedRenderers = 0;
		}
	}

	const uint32_t needed = static_cast<uint32_t>(group.instanceData.size());
	const uint32_t slot = group.usedRenderers++;
	if (slot >= group.renderers.size())
	{
		group.renderers.resize(slot + 1);
	}
	std::unique_ptr<InstancedModelRenderer>& renderer = group.renderers[slot];
	// 足りないバッファは作り直す。使っていたものの SRV の番号は戻さないが、大きくなる方向にしか作り直さないので数は限られる。
	// 前のフレームの描画がまだ GPU で読んでいる可能性があるので、壊さずに置いておく
	if (!renderer || renderer->GetMaxInstances() < needed)
	{
		const uint32_t capacity = (std::min)(kMaxInstancesPerGroup, RoundUpToPowerOfTwo((std::max)(needed, kMinRendererCapacity)));
		auto created = std::make_unique<InstancedModelRenderer>(capacity);
		created->Initialize(dxCommon_, srvManager_, group.model);
		if (renderer)
		{
			retiredRenderers_.push_back(std::move(renderer));
		}
		renderer = std::move(created);
	}
	return renderer.get();
}

void GameObjectInstancing::DrawForward(Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager)
{
	for (Group* group : activeGroups_)
	{
		InstancedModelRenderer* renderer = AcquireRenderer(*group);
		renderer->UpdateBufferDirect(group->instanceData.data(), static_cast<uint32_t>(group->instanceData.size()));
		renderer->DrawInstanced(camera, lightManager, shadowMapManager);
	}
}

void GameObjectInstancing::DrawGBuffer(Camera* camera)
{
	for (Group* group : activeGroups_)
	{
		InstancedModelRenderer* renderer = AcquireRenderer(*group);
		renderer->UpdateBufferDirect(group->instanceData.data(), static_cast<uint32_t>(group->instanceData.size()));
		renderer->DrawInstancedGBuffer(camera);
	}
}

void GameObjectInstancing::DrawShadow(ShadowMapManager* shadowMapManager)
{
	for (Group* group : activeGroups_)
	{
		InstancedModelRenderer* renderer = AcquireRenderer(*group);
		renderer->UpdateBufferDirect(group->instanceData.data(), static_cast<uint32_t>(group->instanceData.size()));
		// 影のシェーダーはカメラを見ないので渡さない
		renderer->DrawInstancedShadow(nullptr, shadowMapManager);
	}
}
} // namespace KCE
