#pragma once
#include <memory>
#include "math/Vector3.h"
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief エリア表示エフェクトクラス
 * 
 * 指定された位置にエリアを示すパーティクルエフェクトを表示します。
 * グラデーションテクスチャを使用した加算合成による光の表現を行います。
 */
class AreaEffect
{
public:
	/**
	 * @brief エフェクトの初期化
	 * 
	 * パーティクルエミッターを作成し、各種モジュールを設定します。
	 * 
	 * @param rotate エフェクトの回転値（現在未使用）
	 * @param scale エフェクトのスケール値
	 */
	void Initialize(const Vector3& rotate, const Vector3& scale);
	
	/**
	 * @brief エフェクトの再生
	 * 
	 * 指定された位置でエフェクトを表示します。
	 * 
	 * @param position エフェクトを表示する位置
	 */
	void Play(const Vector3& position);
	
	/**
	 * @brief エフェクトの停止
	 * 
	 * エフェクトを画面外に移動させて非表示にします。
	 */
	void Stop();

private:
	std::string emitterName_;                         ///< エミッター名（ユニークID付き）
	static inline uint32_t areaEffectCount_ = 0;      ///< エフェクトインスタンスのカウンター
};

