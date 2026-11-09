#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace KCE
{
class Sprite;

/**
 * @brief 名前を付けて置いた 2D スプライトを、まとめて描く場所
 *
 * - Level に置いたスプライトと、シーケンサのスプライトトラックの受け皿
 * - 描くのは Sprite2DPass。シーンの 2D より奥に描く
 * - スプライトの所有は登録した側。破棄する前に Unregister すること
 * - 描く順は order の小さい順（同じならの登録順）
 */
class SpriteLayer
{
public:
	/**
	 * @param name 一意な名前。トラックはこの名前で探す
	 * @param sprite 所有しない
	 * @param order 小さいほど奥
	 */
	void Register(const std::string& name, Sprite* sprite, int order = 0);
	void Unregister(Sprite* sprite);
	/** @brief 登録名を変える。ほかと重なるなら変えずに偽 */
	bool Rename(Sprite* sprite, const std::string& newName);
	void SetOrder(Sprite* sprite, int order);

	/** @return 見つからなければ nullptr */
	Sprite* Find(const std::string& name) const;

	template<class F>
	void ForEachName(F&& callback) const
	{
		for (const Item& item : items_) { callback(item.name); }
	}

	/** @brief 全部更新して描く。Sprite2DPass が呼ぶ */
	void Draw();

private:
	struct Item
	{
		std::string name;
		Sprite* sprite = nullptr;
		int order = 0;
	};
	std::vector<Item> items_;
	// 描く順の作業用。毎フレーム確保しないよう使い回す
	std::vector<Item> sorted_;
};
} // namespace KCE
