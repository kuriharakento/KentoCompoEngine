#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief アサルトライフル命中エフェクトクラス
 * 
 * アサルトライフルが敵に命中した際に表示されるバーストエフェクトです。
 * スケールオーバーライフタイムとカラーフェードを組み合わせた爆発的な表現を行います。
 */
class AssaultRifleHitEffect
{
public:
	/**
	 * @brief エフェクトの初期化
	 * 
	 * パーティクルエミッターを作成し、バースト生成モジュールと
	 * カラーフェード、スケール変化のモジュールを設定します。
	 */
	void Initialize();
	
	/**
	 * @brief エフェクトの再生
	 * 
	 * 指定された位置でエフェクトをバースト再生します。
	 * 
	 * @param position エフェクトを表示する位置
	 */
	void Play(const Vector3& position);

private:
	std::string emitterName_;                   ///< エミッター名（ユニークID付き）
	static inline uint32_t effectCount_ = 0;    ///< エフェクトインスタンスのカウンター
};

