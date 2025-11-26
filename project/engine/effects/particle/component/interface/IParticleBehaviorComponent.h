#pragma once
#include "IParticleComponent.h"
#include "base/GraphicsTypes.h"

/**
 * @brief パーティクル単体に作用するコンポーネントのインターフェース
 * 
 * 個々のパーティクルに対して振る舞いを適用するためのインターフェース。
 * 速度変化、回転、カラー変化など、パーティクル単体への処理を実装する際に継承する。
 */
class IParticleBehaviorComponent : virtual public IParticleComponent
{
public:
	/**
	 * @brief 仮想デストラクタ
	 */
	virtual ~IParticleBehaviorComponent() = default;

	/**
	 * @brief パーティクルを更新する
	 * @param particle 更新対象のパーティクル
	 */
	virtual void Update(Particle& particle) = 0;
};
