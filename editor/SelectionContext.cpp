#include "editor/SelectionContext.h"

#include <algorithm>

#include "gameobject/base/GameObject.h"
#include "gameobject/manager/GameObjectManager.h"

namespace KCE
{
std::unique_ptr<SelectionContext> SelectionContext::instance_ = nullptr;
const SelectionItem SelectionContext::kEmptyItem = SelectionItem{};

SelectionContext* SelectionContext::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<SelectionContext>();
	}
	return instance_.get();
}

bool SelectionContext::HasInstance()
{
	return instance_ != nullptr;
}

void SelectionContext::Finalize()
{
	items_.clear();
	instance_.reset();
}

void SelectionContext::Select(const SelectionItem& item)
{
	// 既に同じものだけが選ばれているなら通知を出さない
	if (items_.size() == 1 && items_.front() == item)
	{
		return;
	}

	items_.clear();
	if (item.kind != SelectionKind::None)
	{
		items_.push_back(item);
	}
	++revision_;
}

void SelectionContext::SelectGameObject(GameObject* gameObject)
{
	if (!gameObject)
	{
		ClearSelection();
		return;
	}

	SelectionItem item;
	item.kind = SelectionKind::GameObject;
	item.objectGuid = gameObject->GetGuid();
	Select(item);
}

void SelectionContext::AddToSelection(const SelectionItem& item)
{
	if (item.kind == SelectionKind::None || IsSelected(item))
	{
		return;
	}

	items_.push_back(item);
	++revision_;
}

void SelectionContext::ToggleSelection(const SelectionItem& item)
{
	if (IsSelected(item))
	{
		RemoveFromSelection(item);
	}
	else
	{
		AddToSelection(item);
	}
}

void SelectionContext::RemoveFromSelection(const SelectionItem& item)
{
	const auto it = std::find(items_.begin(), items_.end(), item);
	if (it == items_.end())
	{
		return;
	}

	items_.erase(it);
	++revision_;
}

void SelectionContext::ClearSelection()
{
	if (items_.empty())
	{
		return;
	}

	items_.clear();
	++revision_;
}

bool SelectionContext::IsSelected(const SelectionItem& item) const
{
	return std::find(items_.begin(), items_.end(), item) != items_.end();
}

const SelectionItem& SelectionContext::GetPrimary() const
{
	return items_.empty() ? kEmptyItem : items_.back();
}

GameObject* SelectionContext::GetPrimaryGameObject() const
{
	const SelectionItem& primary = GetPrimary();
	if (primary.kind != SelectionKind::GameObject || !GameObjectManager::HasInstance())
	{
		return nullptr;
	}

	return GameObjectManager::GetInstance()->FindByGuid(primary.objectGuid);
}

std::vector<GameObject*> SelectionContext::GetSelectedGameObjects() const
{
	std::vector<GameObject*> result;
	if (!GameObjectManager::HasInstance())
	{
		return result;
	}

	auto* manager = GameObjectManager::GetInstance();
	for (const auto& item : items_)
	{
		if (item.kind != SelectionKind::GameObject)
		{
			continue;
		}

		// 破棄済みのオブジェクトは解決できないので自然に除外される
		if (auto* object = manager->FindByGuid(item.objectGuid))
		{
			result.push_back(object);
		}
	}
	return result;
}
} // namespace KCE
