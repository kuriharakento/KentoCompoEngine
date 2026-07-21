#pragma once
#include "engine/scene/interface/AbstractSceneFactory.h"
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <memory>

/**
 * @brief 動的レジストリ機能を持つシーンファクトリー
 */
class SceneFactory : public AbstractSceneFactory
{
public:
	using Creator = std::function<std::unique_ptr<BaseScene>()>;

	/**
	 * @brief シーンの生成関数を登録する。
	 * @param sceneName シーン識別名
	 * @param creator シーン生成関数
	 */
	static void RegisterScene(const std::string& sceneName, Creator creator)
	{
		GetRegistry()[sceneName] = creator;
	}

	/**
	 * @brief シーン型をテンプレート指定して登録する。
	 * @tparam T 登録するシーンクラス型
	 * @param sceneName シーン識別名
	 * @return 登録に成功したら true
	 */
	template <typename T>
	static bool Register(const std::string& sceneName)
	{
		RegisterScene(sceneName, []() -> std::unique_ptr<BaseScene>
		{
			return std::make_unique<T>();
		});
		return true;
	}

	/**
	 * @brief 登録済みの全シーン名リストを取得する（ImGuiデバッグUI等用）。
	 * @return シーン識別名のリスト
	 */
	static std::vector<std::string> GetRegisteredSceneNames()
	{
		std::vector<std::string> names;
		for (const auto& [name, creator] : GetRegistry())
		{
			names.push_back(name);
		}
		return names;
	}

	/**
	 * @brief シーン生成。
	 * @param sceneName シーン識別名
	 * @return 生成されたシーン
	 */
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;

private:
	/**
	 * @brief レジストリ（マップ）の参照取得。静的初期化順序問題を避けるため static local。
	 */
	static std::unordered_map<std::string, Creator>& GetRegistry()
	{
		static std::unordered_map<std::string, Creator> registry;
		return registry;
	}
};

/**
 * @brief シーンクラスを自動登録するマクロ。
 * 各シーンの .cpp ファイルの Initialize() 直前に記述する。
 */
#define REGISTER_SCENE(SceneClass) \
	namespace { [[maybe_unused]] const bool is##SceneClass##Registered = SceneFactory::Register<SceneClass>(#SceneClass); }

