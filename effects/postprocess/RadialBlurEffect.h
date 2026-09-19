#pragma once
#include "base/BasePostEffect.h"
#include "math/Vector2.h"

namespace KCE
{
/** @brief 指定した中心から外へ伸びるズームブラーを掛ける。 */
class RadialBlurEffect : public BasePostEffect
{
public:
	/** @brief 初期値で無効なラジアルブラーを作る。 */
	RadialBlurEffect();
	/** @brief 定数バッファへ現在値を反映する。 */
	void ApplyEffect(PostEffectParams& params) override;
	/** @brief UV空間の中心を設定する。 */
	void SetCenter(const Vector2& center);
	/** @brief UV空間の中心を返す。 */
	Vector2 GetCenter() const { return center_; }
	/** @brief 放射方向の長さを設定する。 */
	void SetStrength(float strength);
	/** @brief 放射方向の長さを返す。 */
	float GetStrength() const { return strength_; }
	/** @brief サンプル数を設定する。 */
	void SetSampleCount(int sampleCount);
	/** @brief サンプル数を返す。 */
	int GetSampleCount() const { return sampleCount_; }
	/** @brief 元の絵への混ぜ具合を設定する。 */
	void SetBlend(float blend);
	/** @brief 元の絵への混ぜ具合を返す。 */
	float GetBlend() const { return blend_; }

private:
	Vector2 center_ = { 0.5f, 0.5f };
	float strength_ = 0.05f;
	int sampleCount_ = 8;
	// 1.0 で全部ブラー、0.0 で元の絵のまま
	float blend_ = 1.0f;
};
} // namespace KCE
