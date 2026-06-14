#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <type_traits>

class GameObject;

namespace GameObjectComponent
{
	class IGameObjectComponent;

	/**
	 * @brief コンポーネント自動登録ファクトリクラス（シングルトン）
	 */
	class ComponentFactory
	{
	public:
		static ComponentFactory* GetInstance()
		{
			static ComponentFactory instance;
			return &instance;
		}

		/**
		 * @brief コンポーネント生成関数の登録
		 */
		void Register(const std::string& typeName, std::function<std::unique_ptr<IGameObjectComponent>(GameObject*)> creator)
		{
			registry_[typeName] = creator;
		}

		/**
		 * @brief コンポーネントのインスタンス作成
		 */
		std::unique_ptr<IGameObjectComponent> Create(const std::string& typeName, GameObject* owner)
		{
			auto it = registry_.find(typeName);
			if (it != registry_.end())
			{
				return it->second(owner);
			}
			return nullptr;
		}

		/**
		 * @brief 登録済みコンポーネント名の一覧を取得
		 */
		std::vector<std::string> GetRegisteredNames() const
		{
			std::vector<std::string> names;
			for (const auto& [name, _] : registry_)
			{
				names.push_back(name);
			}
			return names;
		}

	private:
		ComponentFactory() = default;
		~ComponentFactory() = default;
		ComponentFactory(const ComponentFactory&) = delete;
		ComponentFactory& operator=(const ComponentFactory&) = delete;

	private:
		std::unordered_map<std::string, std::function<std::unique_ptr<IGameObjectComponent>(GameObject*)>> registry_;
	};

	/**
	 * @brief 自動登録用ヘルパークラス（テンプレート）
	 */
	template <typename T>
	class ComponentRegisterer
	{
	public:
		ComponentRegisterer(const std::string& typeName)
		{
			ComponentFactory::GetInstance()->Register(typeName, [](GameObject* owner) -> std::unique_ptr<IGameObjectComponent> {
				// コンストラクタ引数があるかどうかをメタプログラミングで判定して生成
				if constexpr (std::is_constructible_v<T, GameObject*>)
				{
					return std::make_unique<T>(owner);
				}
				else
				{
					return std::make_unique<T>();
				}
			});
		}
	};
}

/**
 * @brief コンポーネントを起動時に自動登録するためのマクロ
 */
#define REGISTER_COMPONENT(Type) \
	static GameObjectComponent::ComponentRegisterer<GameObjectComponent::Type> g_registerer_##Type(#Type);
