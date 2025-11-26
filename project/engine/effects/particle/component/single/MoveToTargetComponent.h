#pragma once
#include "effects/particle/component/interface/IParticleBehaviorComponent.h"

/**
 * @brief パーティクルを指定したターゲット位置に移動させるコンポーネント
 * 
 * パーティクルの寿命に基づいてイージング関数を使用し、
 * 開始位置からターゲット位置へスムーズに補間移動させる。
 * 静的なターゲット座標またはポインタによる動的なターゲットをサポートする。
 */
class MoveToTargetComponent : public IParticleBehaviorComponent
{
public:
    /**
     * @brief コンストラクタ（静的ターゲット）
     * @param target 移動先のターゲット座標
     * @param speed 移動速度（現在は補間で移動するため未使用）
     */
	MoveToTargetComponent(const Vector3& target, const float& speed);

    /**
     * @brief コンストラクタ（動的ターゲット）
     * @param target 移動先のターゲット座標へのポインタ（毎フレーム参照）
     * @param speed 移動速度（現在は補間で移動するため未使用）
     */
	MoveToTargetComponent(const Vector3* target, const float& speed);

    /**
     * @brief パーティクルの位置をターゲットに向けて更新する
     * @param particle 更新対象のパーティクル
     */
	void Update(Particle& particle) override;

private:
    // 移動先のターゲット座標
	Vector3 target_;
    // 動的ターゲット座標へのポインタ（nullptr時は静的ターゲットを使用）
	const Vector3* targetPtr_ = nullptr;
    // 移動速度
	float speed_ = 0.0f;
};

