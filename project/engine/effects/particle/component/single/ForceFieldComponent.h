#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

// math
#include "base/GraphicsTypes.h"

/**
 * @brief 特定の中心点からの引力または斥力をパーティクルに与えるコンポーネント
 * 
 * 指定された中心点からパーティクルに対して引力または斥力を適用する。
 * 力の強さは距離に反比例し、逆二乗の法則に基づく自然な力場を再現する。
 */
class ForceFieldComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief 力場の種類を表す列挙型
     */
    enum class ForceType
    {
        // 引力（中心に引き寄せる）
        Attract,
        // 斥力（中心から遠ざける）
        Repel
    };

    /**
     * @brief コンストラクタ
     * @param center 力場の中心座標
     * @param strength 力の強さ
     * @param maxDistance 力が及ぶ最大距離（この距離外のパーティクルには影響しない）
     * @param type 力の種類（引力または斥力、デフォルトは引力）
     */
    explicit ForceFieldComponent(const Vector3& center, float strength, float maxDistance, ForceType type = ForceType::Attract);

    /**
     * @brief パーティクルに力場の影響を与える
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;

private:
    // 力場の中心座標
    Vector3 forceCenter;
    // 力の強さ
    float strength;
    // 力が及ぶ最大距離
    float maxDistance;
    // 力の種類（引力または斥力）
    ForceType type;
};