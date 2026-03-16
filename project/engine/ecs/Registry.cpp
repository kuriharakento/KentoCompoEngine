#include "Registry.h"

void Registry::Initialize(uint32_t maxEntities)
{
    m_maxEntities = maxEntities;
    m_generations.resize(maxEntities, 0);

    // 空きIndexキューを初期化（最初は0から昇順）
    std::queue<uint32_t> emptyQueue;
    std::swap(m_availableIndices, emptyQueue); // clear
    for (uint32_t i = 0; i < maxEntities; ++i)
    {
        m_availableIndices.push(i);
    }
}

EntityID Registry::CreateEntity()
{
    if (m_availableIndices.empty())
    {
        // 動的拡張は行わないポリシー
        std::cerr << "[Registry] Error: Maximum entity count reached (" << m_maxEntities << ")!\n";
        return kInvalidEntity;
    }

    uint32_t index = m_availableIndices.front();
    m_availableIndices.pop();

    // 割り当てのたびに世代が上がる（1は使わない初期値0との区別のため初回で1になる）
    m_generations[index]++;

    m_activeEntityCount++;
    return MakeEntityID(index, m_generations[index]);
}

bool Registry::IsAlive(EntityID entity) const
{
    if (entity == kInvalidEntity) return false;
    
    uint32_t index = GetEntityIndex(entity);
    if (index >= m_maxEntities) return false;
    
    // 要求されたEntityIDの世代と、現在そのIndexが持っている世代が一致するか
    return m_generations[index] == GetEntityGeneration(entity);
}

void Registry::DestroyEntityDeferred(EntityID entity)
{
    if (IsAlive(entity))
    {
        m_destroyQueue.push_back(entity);
    }
}

void Registry::FlushGarbageCollection()
{
    for (EntityID entity : m_destroyQueue)
    {
        // 既にキュー内で重複して呼ばれたりした場合の安全策
        if (!IsAlive(entity)) continue;

        uint32_t index = GetEntityIndex(entity);

        // 1. 各ComponentArrayからこのEntityの持つデータを物理削除（Swap & Pop）
        for (auto& pair : m_componentArrays)
        {
            pair.second->EntityDestroyed(entity);
        }

        // 2. 世代を進めて破棄済み（Invalid）状態にする
        // 次にリサイクルされた時に世代が異なるため、古いポインタを弾けるようになる
        m_generations[index]++; 

        // 3. スロットを空きキューに返却
        m_availableIndices.push(index);

        m_activeEntityCount--;
    }

    m_destroyQueue.clear();
}
