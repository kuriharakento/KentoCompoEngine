#include "Registry.h"

void Registry::Initialize(uint32_t maxEntities)
{
    maxEntities_ = maxEntities;
    generations_.resize(maxEntities, 0);

    // 物理メモリのページを強制的に確定させる
    std::fill(generations_.begin(), generations_.end(), 0);

    // 空きIndexキューを初期化
    std::queue<uint32_t> emptyQueue;
    std::swap(availableIndices_, emptyQueue);
    for (uint32_t i = 0; i < maxEntities; ++i)
    {
        availableIndices_.push(i);
    }
}

Entity Registry::Create()
{
    return Entity(CreateEntity(), this);
}

EntityID Registry::CreateEntity()
{
    if (availableIndices_.empty())
    {
        // 最大数に達した場合はエラー
        return kInvalidEntity;
    }

    uint32_t index = availableIndices_.front();
    availableIndices_.pop();

    // 割り当てのたびに世代を上げる（初期値0と区別するため初回で1にする）
    generations_[index]++;

    activeEntityCount_++;
    return MakeEntityID(index, generations_[index]);
}

bool Registry::IsAlive(EntityID entity) const
{
    if (entity == kInvalidEntity)
    {
        return false;
    }

    uint32_t index = GetEntityIndex(entity);
    if (index >= maxEntities_)
    {
        return false;
    }

    // 世代が一致するかで生存判定
    return generations_[index] == GetEntityGeneration(entity);
}

void Registry::DestroyEntityDeferred(EntityID entity)
{
    if (IsAlive(entity))
    {
        destroyQueue_.push_back(entity);
    }
}

void Registry::FlushGarbageCollection()
{
    for (EntityID entity : destroyQueue_)
    {
        if (!IsAlive(entity))
        {
            continue;
        }

        uint32_t index = GetEntityIndex(entity);

        // 各コンポーネント配列からデータを削除
        for (auto& pair : componentArrays_)
        {
            pair.second->EntityDestroyed(entity);
        }

        // 世代を進めて無効化する
        generations_[index]++;

        // インデックスを再利用キューへ
        availableIndices_.push(index);

        activeEntityCount_--;
    }

    destroyQueue_.clear();
}

