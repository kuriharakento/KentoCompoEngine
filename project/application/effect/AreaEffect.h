#pragma once
#include "effects/particle/ParticleEmitter.h"

/**
 * @brief エリア範囲表示エフェクトクラス
 * 
 * ゲーム内で特定の範囲やエリアを視覚的に示すためのエフェクトを管理します。
 * キューブ型のパーティクルとUVアニメーションを使用して、目立つエリアマーカーを表現します。
 * 
 * 主な機能:
 * - カスタマイズ可能な回転とスケール
 * - 自動的なUVスクロールアニメーション
 * - フェードアウトによる自然な消滅
 * - 連続発生による持続的な表示
 * 
 * @code
 * // 使用例
 * AreaEffect areaEffect;
 * Vector3 rotation = {0, 0, 0};
 * Vector3 scale = {1, 1, 1};
 * areaEffect.Initialize(rotation, scale);
 * areaEffect.Play(effectPosition);  // エフェクト開始
 * areaEffect.Stop();  // エフェクト停止
 * @endcode
 */
class AreaEffect
{
public:
	/**
	 * @brief エフェクトの初期化
	 * 
	 * エリアエフェクトのエミッターを初期化し、回転とスケールを設定します。
	 * キューブ型のパーティクルを使用し、UVスクロールコンポーネントを追加します。
	 * 
	 * @param rotate エフェクトの回転（ラジアン）
	 * @param scale エフェクトのスケール倍率
	 */
	void Initialize(const Vector3& rotate, const Vector3& scale);

	/**
	 * @brief エフェクトの再生
	 * 
	 * 指定位置でエリアエフェクトを開始します。
	 * 連続的にパーティクルを発生させます。
	 * 
	 * @param position エフェクト発生位置
	 */
	void Play(const Vector3& position);
	
	/**
	 * @brief エフェクトの停止
	 * 
	 * 新規パーティクルの発生を停止します。
	 * 既存のパーティクルは自然に消滅します。
	 */
	void Stop()
	{
		areaEmitter_->StopEmit();
	}

private:
	std::unique_ptr<ParticleEmitter> areaEmitter_;  ///< エリア表示用パーティクルエミッター
};

