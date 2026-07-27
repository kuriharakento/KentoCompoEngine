#pragma once

#include "Registry.h"
#include "Entity.h"

namespace KCE
{
/**
 * @brief メソッドチェーンで直感的にEntityを構築するためのビルダー
 *
 * 例:
 * Entity entity = EntityBuilder(registry)
 *     .Create()
 *     .AddTransform({0, 10, 0})
 *     .Add(ecs::TagComponent{ ecs::TagComponent::Type::Player })
 *     .Build();
 */
class EntityBuilder
{
public:
	explicit EntityBuilder(Registry& registry) : registry_(registry) {}

	/**
	 * @brief 新しいEntityを発行し、構築を開始する
	 */
	EntityBuilder& Create()
	{
		entity_ = registry_.Create();
		return *this;
	}

	/**
	 * @brief 既存のEntityの構築を再開する
	 */
	EntityBuilder& Edit(Entity entity)
	{
		entity_ = entity;
		return *this;
	}

	/**
	 * @brief コンポーネントを追加する汎用メソッド
	 */
	template<typename T>
	EntityBuilder& Add(T component)
	{
		entity_.Add(std::move(component));
		return *this;
	}

	/**
	 * @brief 構築したEntityを返す
	 */
	Entity Build()
	{
		return entity_;
	}

private:
	Registry& registry_;
	Entity entity_;
};
} // namespace KCE
