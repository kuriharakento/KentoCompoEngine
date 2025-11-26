#pragma once
#include <cstdlib>
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルにランダムな初速度を与えるコンポーネント
 * 
 * パーティクルの初回更新時にのみランダムな速度を設定する。
 * 指定された最小値と最大値の範囲内で各軸ごとにランダムな値を生成する。
 */
class RandomInitialVelocityComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param minV 速度の最小値ベクトル
     * @param maxV 速度の最大値ベクトル
     */
    RandomInitialVelocityComponent(const Vector3& minV, const Vector3& maxV);

    /**
     * @brief パーティクルに初期速度を設定する（初回のみ）
     * @param particle 更新対象のパーティクル
     */
    void Update(Particle& particle) override;

private:
    /**
     * @brief 指定範囲のランダムな浮動小数点数を生成する
     * @param min 最小値
     * @param max 最大値
     * @return min〜maxの範囲のランダム値
     */
    float RandomFloat(float min, float max);

    // 速度の最小値ベクトル
    Vector3 minVelocity_;
    // 速度の最大値ベクトル
    Vector3 maxVelocity_;
    // 初期化済みフラグ（trueの場合は速度を再設定しない）
    bool initialized_ = false;
};
