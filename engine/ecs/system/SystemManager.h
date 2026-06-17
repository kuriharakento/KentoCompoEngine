#pragma once

#include "ISystem.h"
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <cassert>

/**
 * @brief ECSシステムを一括管理・実行するクラス。
 */
class SystemManager
{
public:
    SystemManager() = default;
    ~SystemManager() { Finalize(); }

    /**
     * @brief システムを登録する（所有権を譲受）。
     * @param system 登録するシステムのインスタンス
     */
    void AddSystem(std::unique_ptr<ISystem> system);

    /**
     * @brief 指定した型のシステムを取得する。
     * @tparam T システム型
     * @return システムへのポインタ（非所有）。存在しなければ nullptr。
     */
    template <typename T>
    T* GetSystem() const
    {
        auto it = systemMap_.find(std::type_index(typeid(T)));
        if (it != systemMap_.end())
        {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }

    /**
     * @brief 全システムの初期化。
     */
    void Initialize();

    /**
     * @brief 全システムの更新。
     * @param registry Registry
     */
    void Update(Registry& registry);

    /**
     * @brief 全システムの描画。
     */
    void Draw(Registry& registry, Camera* camera, LightManager* lightManager, ShadowMapManager* shadowMapManager);

    /**
     * @brief 全システムのシャドウ描画。
     */
    void DrawShadow(Registry& registry);

    /**
     * @brief 全システムの終了処理。
     */
    void Finalize();

private:
    // 実行順序保持リスト (所有)
    std::vector<std::unique_ptr<ISystem>> systems_;
    // 型による逆引きマップ (非所有)
    std::unordered_map<std::type_index, ISystem*> systemMap_;
};

