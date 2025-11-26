#pragma once

/**
 * @brief パーティクルコンポーネントの基底インターフェース
 * 
 * Component-Based Designパターンに基づくパーティクルシステムの基底クラス。
 * すべてのパーティクルコンポーネントはこのインターフェースを継承する。
 */
class IParticleComponent
{
public:
    /**
     * @brief 仮想デストラクタ
     */
    virtual ~IParticleComponent() = default;
};