#pragma once
#include "IParticleComponent.h"

class ParticleGroup;

/**
 * @brief パーティクルグループ全体に作用するコンポーネントのインターフェース
 * 
 * パーティクルグループ全体に対して振る舞いを適用するためのインターフェース。
 * UVアニメーション、マテリアルカラー変更など、グループ全体への処理を実装する際に継承する。
 */
class IParticleGroupComponent : virtual  public IParticleComponent
{
public:
	/**
	 * @brief 仮想デストラクタ
	 */
	virtual ~IParticleGroupComponent() = default;

	/**
	 * @brief パーティクルグループを更新する
	 * @param group 更新対象のパーティクルグループ
	 */
    virtual void Update(ParticleGroup& group) = 0;
};
