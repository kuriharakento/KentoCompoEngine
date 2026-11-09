#include "graphics/2d/SpriteLayer.h"

#include "graphics/2d/Sprite.h"

namespace KCE
{
void SpriteLayer::Register(const std::string& name, Sprite* sprite, int order)
{
	if (!sprite)
	{
		return;
	}
	for (Item& item : items_)
	{
		if (item.sprite == sprite)
		{
			item.name = name;
			item.order = order;
			return;
		}
	}
	items_.push_back({ name, sprite, order });
}

void SpriteLayer::Unregister(Sprite* sprite)
{
	items_.erase(std::remove_if(items_.begin(), items_.end(), [sprite](const Item& item) { return item.sprite == sprite; }), items_.end());
}

bool SpriteLayer::Rename(Sprite* sprite, const std::string& newName)
{
	if (Find(newName) && Find(newName) != sprite)
	{
		return false;
	}
	for (Item& item : items_)
	{
		if (item.sprite == sprite)
		{
			item.name = newName;
			return true;
		}
	}
	return false;
}

void SpriteLayer::SetOrder(Sprite* sprite, int order)
{
	for (Item& item : items_)
	{
		if (item.sprite == sprite) { item.order = order; }
	}
}

Sprite* SpriteLayer::Find(const std::string& name) const
{
	for (const Item& item : items_)
	{
		if (item.name == name) { return item.sprite; }
	}
	return nullptr;
}

void SpriteLayer::Draw()
{
	if (items_.empty())
	{
		return;
	}
	sorted_ = items_;
	std::stable_sort(sorted_.begin(), sorted_.end(), [](const Item& a, const Item& b) { return a.order < b.order; });
	for (const Item& item : sorted_)
	{
		item.sprite->Update();
		item.sprite->Draw();
	}
}
} // namespace KCE
