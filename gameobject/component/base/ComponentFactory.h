#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <type_traits>
#include <typeindex>

namespace KCE
{
class GameObject;

namespace GameObjectComponent
{
	class Component;

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
		void Register(const std::string& typeName, std::function<std::unique_ptr<Component>(GameObject*)> creator)
		{
			registry_[typeName] = creator;
		}

		template<typename T>
		void RegisterTypeName(const std::string& typeName)
		{
			// 正規名の登録がaliasより先に並ぶ。最初の名前を保存名として固定する
			typeNames_.try_emplace(std::type_index(typeid(T)), typeName);
		}

		std::string GetTypeName(const std::type_info& type) const
		{
			const auto found = typeNames_.find(std::type_index(type));
			return found != typeNames_.end() ? found->second : type.name();
		}

		/**
		 * @brief コンポーネントのインスタンス作成
		 */
		std::unique_ptr<Component> Create(const std::string& typeName, GameObject* owner)
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
		std::unordered_map<std::string, std::function<std::unique_ptr<Component>(GameObject*)>> registry_;
		std::unordered_map<std::type_index, std::string> typeNames_;
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
			ComponentFactory::GetInstance()->RegisterTypeName<T>(typeName);
			ComponentFactory::GetInstance()->Register(typeName, [](GameObject* owner) -> std::unique_ptr<Component> {
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
	static KCE::GameObjectComponent::ComponentRegisterer<KCE::GameObjectComponent::Type> g_registerer_##Type(#Type);
#define REGISTER_COMPONENT_ALIAS(Type, Alias) \
	static KCE::GameObjectComponent::ComponentRegisterer<KCE::GameObjectComponent::Type> g_registerer_##Type##_##Alias(#Alias);
} // namespace KCE
