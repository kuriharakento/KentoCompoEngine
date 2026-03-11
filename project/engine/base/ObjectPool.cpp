#include "ObjectPool.h"
#include <cassert>

// 前方宣言（明示的インスタンス化のため）
class ParticleEffect;
// class Bullet; // 今後必要になったら追加

template<class T>
void ObjectPool<T>::Initialize(uint32_t capacity, OverflowPolicy policy)
{
	capacity_ = capacity;
	policy_ = policy;
	activeCount_ = 0;

	items_.resize(capacity);
	freeIndices_.resize(capacity);
	
	// 空きインデックスを逆順に詰める（0番が最初に取り出されるように）
	for (uint32_t i = 0; i < capacity; ++i)
	{
		freeIndices_[i] = capacity - 1 - i;
		items_[i].generation = 1; // 初期世代は1
	}

	// Recycle用にアクティブキューを初期化
	if (policy_ == OverflowPolicy::Recycle)
	{
		activeQueue_.resize(capacity);
		activeQueueHead_ = 0;
		activeQueueTail_ = 0;
	}
}

template<class T>
typename ObjectPool<T>::Handle ObjectPool<T>::Acquire()
{
	if (freeIndices_.empty())
	{
		switch (policy_)
		{
		case OverflowPolicy::Fail:
			// 枯渇したので、無効なハンドルを返す
			return Handle{ Handle::kInvalid, 0 };

		case OverflowPolicy::Grow:
			// 動的に容量を拡張する
			ExpandCapacity(capacity_ > 0 ? capacity_ * 2 : 64);
			break;

		case OverflowPolicy::Recycle:
			// 最古の要素を上書きする
			return RecycleOldest();
		}
	}

	// 空きインデックスを取得
	uint32_t index = freeIndices_.back();
	freeIndices_.pop_back();

	// アイテムの世代をインクリメント（使い回しを検知するため）
	items_[index].generation++;
	activeCount_++;

	// Recycle用のキューに積む
	if (policy_ == OverflowPolicy::Recycle)
	{
		activeQueue_[activeQueueTail_] = index;
		activeQueueTail_ = (activeQueueTail_ + 1) % activeQueue_.size();
	}

	return Handle{ index, items_[index].generation };
}

template<class T>
void ObjectPool<T>::Release(Handle handle)
{
	if (!handle.IsValid() || handle.index >= items_.size())
	{
		return; // 無効なハンドル
	}

	// 世代が一致しない場合は既に返却済みか他の用途で使われている
	if (items_[handle.index].generation != handle.generation)
	{
		return;
	}

	// 世代を進めて、このハンドルを利用不可にする
	items_[handle.index].generation++;
	
	freeIndices_.push_back(handle.index);
	activeCount_--;
}

template<class T>
T* ObjectPool<T>::Get(Handle handle)
{
	if (!handle.IsValid() || handle.index >= items_.size())
	{
		return nullptr;
	}

	if (items_[handle.index].generation != handle.generation)
	{
		// 古いハンドルでのアクセスをブロック（ダングリング防止）
		return nullptr;
	}

	return &items_[handle.index].payload;
}

template<class T>
typename ObjectPool<T>::Stats ObjectPool<T>::GetStats() const
{
	return { activeCount_, static_cast<uint32_t>(freeIndices_.size()), capacity_ };
}

template<class T>
void ObjectPool<T>::Clear()
{
	activeCount_ = 0;
	freeIndices_.clear();

	// 全インデックスを空きリストに戻し、世代を更新する
	for (uint32_t i = 0; i < capacity_; ++i)
	{
		freeIndices_.push_back(capacity_ - 1 - i);
		items_[i].generation++; // 一斉に世代を進めて既存ハンドルを無効化
	}

	if (policy_ == OverflowPolicy::Recycle)
	{
		activeQueueHead_ = 0;
		activeQueueTail_ = 0;
	}
}

template<class T>
void ObjectPool<T>::ExpandCapacity(uint32_t newCapacity)
{
	if (newCapacity <= capacity_) return;

	items_.resize(newCapacity);
	
	// 新しく増えた分を空きリストに追加
	uint32_t addCount = newCapacity - capacity_;
	for (uint32_t i = 0; i < addCount; ++i)
	{
		uint32_t newIndex = capacity_ + i;
		freeIndices_.push_back(newIndex);
		items_[newIndex].generation = 1;
	}

	// activeQueue_ の拡張も必要
	if (policy_ == OverflowPolicy::Recycle)
	{
		std::vector<uint32_t> newQueue(newCapacity);
		// 既存のキュー要素を先頭から詰め直す
		uint32_t count = 0;
		uint32_t i = activeQueueHead_;
		while (i != activeQueueTail_)
		{
			newQueue[count++] = activeQueue_[i];
			i = (i + 1) % activeQueue_.size();
		}
		activeQueue_ = std::move(newQueue);
		activeQueueHead_ = 0;
		activeQueueTail_ = count;
	}

	capacity_ = newCapacity;
}

template<class T>
typename ObjectPool<T>::Handle ObjectPool<T>::RecycleOldest()
{
	assert(activeQueueHead_ != activeQueueTail_ && "Queue is empty but trying to recycle!");

	// キューから最も古いインデックスを取り出す
	uint32_t oldIndex = activeQueue_[activeQueueHead_];
	activeQueueHead_ = (activeQueueHead_ + 1) % activeQueue_.size();

	// その要素を強奪し、世代を進める
	items_[oldIndex].generation++;
	
	// 再びキューの末尾に積む（再利用された要素が最も「新しい」要素になる）
	activeQueue_[activeQueueTail_] = oldIndex;
	activeQueueTail_ = (activeQueueTail_ + 1) % activeQueue_.size();

	// アクティブ数は変わらない（1つ奪って1つ返すため）

	return Handle{ oldIndex, items_[oldIndex].generation };
}

// =========================================================================
// 明示的テンプレートインスタンス化
// =========================================================================

#include "effects/particle/ParticleEffect.h"
template class ObjectPool<ParticleEffect>;

// 今後、Enemy や Bullet 等でプーリングを使用する場合は、以下に追記していく
// #include "gameobject/enemy/Enemy.h"
// template class ObjectPool<Enemy>;
