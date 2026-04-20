#include "Registry.h"

void Registry::Initialize(uint32_t maxEntities)
{
    maxEntities_ = maxEntities;
    generations_.resize(maxEntities, 0);

    // [BNS-Optimization] 物理メモリのページを強制的に確定させる（Pre-touch）
    // これを行わないと、初回スポーン時に OS が物理メモリを割り当てる際のスパイク（カクつき）が発生する
    std::fill(generations_.begin(), generations_.end(), 0);

    // 空きIndexキューを初期化（最初は0から昇順）
    std::queue<uint32_t> emptyQueue;
    std::swap(availableIndices_, emptyQueue); // clear
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
        // 動的拡張は行わないポリシー
        std::cerr << "[Registry] Error: Maximum entity count reached (" << maxEntities_ << ")!\n";
        return kInvalidEntity;
    }

    uint32_t index = availableIndices_.front();
    availableIndices_.pop();

    // 割り当てのたびに世代が上がる（1は使わない初期値0との区別のため初回で1になる）
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

    // 要求されたEntityIDの世代と、現在そのIndexが持っている世代が一致するか
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
        // 既にキュー内で重複して呼ばれたりした場合の安全策
        if (!IsAlive(entity))
        {
            continue;
        }

        uint32_t index = GetEntityIndex(entity);

        // 1. 各ComponentArrayからこのEntityの持つデータを物理削除（Swap & Pop）
        for (auto& pair : componentArrays_)
        {
            pair.second->EntityDestroyed(entity);
        }

        // 2. 世代を進めて破棄済み（Invalid）状態にする
        // 次にリサイクルされた時に世代が異なるため、古いポインタを弾けるようになる
        generations_[index]++;

        // 3. スロットを空きキューに返却
        availableIndices_.push(index);

        activeEntityCount_--;
    }

    destroyQueue_.clear();
}
