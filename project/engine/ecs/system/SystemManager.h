#pragma once

#include "ISystem.h"
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <cassert>

/**
 * @brief 登録されたECS Systemを一括で管理・実行するマネージャークラス
 */
class SystemManager
{
public:
	SystemManager() = default;
	~SystemManager() { Finalize(); }

	/**
	 * @brief システムを登録する
	 * @param system 登録するシステムのインスタンス
	 */
	void AddSystem(std::shared_ptr<ISystem> system);

	/**
	 * @brief 指定した型のシステムを取得する
	 * @tparam T システムの型
	 * @return システムのインスタンス（存在する場合はポインタ、見つからなければnullptr）
	 */
	template <typename T>
	std::shared_ptr<T> GetSystem() const
	{
		auto it = systemMap_.find(std::type_index(typeid(T)));
		if (it != systemMap_.end())
		{
			return std::dynamic_pointer_cast<T>(it->second);
		}
		return nullptr;
	}

	/**
	 * @brief 全システムの初期化
	 */
	void Initialize();

	/**
	 * @brief 全システムの更新
	 * @param registry 実行対象のRegistry
	 */
	void Update(Registry& registry);

	/**
	 * @brief 全システムの描画
	 */
	void Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager);

	/**
	 * @brief 全システムのシャドウ描画
	 */
	void DrawShadow(Registry& registry);

	/**
	 * @brief 全システムの終了処理
	 */
	void Finalize();

private:
	// 実行順序を保証するためのリスト
	std::vector<std::shared_ptr<ISystem>> systems_;
	// クラス型から高速にシステムを逆引きするためのマップ
	std::unordered_map<std::type_index, std::shared_ptr<ISystem>> systemMap_;
};
