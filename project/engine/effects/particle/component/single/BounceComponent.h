#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルの地面との反発（バウンス）を処理するコンポーネント
 * 
 * パーティクルが地面に衝突した際に反発係数に基づいて速度を反転させる。
 * 次フレームでの位置を予測し、地面を貫通する前に衝突を検出する。
 */
class BounceComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ
     * @param groundHeight 地面の高さ（Y座標）
     * @param restitution 反発係数（0.0〜1.0、1.0で完全弾性衝突）
     * @param minVelocity 最小速度（この速度以下で停止とみなす）
     */
	explicit BounceComponent(float groundHeight, float restitution, float minVelocity);

    /**
     * @brief パーティクルの地面衝突を判定し、反発処理を行う
     * @param particle 更新対象のパーティクル
     */
	void Update(Particle& particle) override;

private:
    // 地面の高さ（Y座標）
	float groundHeight_;
    // 反発係数（0.0〜1.0）
	float restitution_;
    // 最小速度閾値
	float minVelocity_;
};

