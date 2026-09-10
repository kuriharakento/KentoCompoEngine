#pragma once
#include "math/Vector4.h"
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief 駆動するライトの種類
 * @details JSON には文字列で保存する。
 */
enum class LightTrackKind
{
	Spot,		 //!< スポットライト（名前で指定）
	Point,		 //!< ポイントライト（名前で指定）
	Directional, //!< 平行光源（シーンに1つ。名前は使わない）
};

/**
 * @brief ライトの色・強さ・形状をカーブで駆動するトラック
 *
 * @details スポット・ポイントは LightManager が名前で管理しているため、
 *          役（ロール）には実体ではなくライト名が割り当てられる。
 *          LightManager 自体は BindingContext から受け取る。
 *
 *          @b 注意: LightManager::GetSpotLight* などは、名前が見つからないと
 *          先頭要素を返す実装であり、ライトが1本も無いと未定義動作になる。
 *          このトラックは必ず存在を確かめてから値を読み書きする。
 */
class LightTrack : public ITrack
{
public:
	LightTrack();

	TrackType GetType() const override { return TrackType::Light; }
	const char* GetTypeName() const override { return "Light"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override { return const_cast<LightTrack*>(this)->GetChannelsEndTime(); }

	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override;
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;

#ifdef USE_IMGUI
	bool DrawInspector() override;
#endif

	LightTrackKind GetKind() const { return kind_; }
	void SetKind(LightTrackKind kind) { kind_ = kind; }

private:
	/**
	 * @brief 役に割り当てられたライトが実在するか
	 * @param ctx バインディングコンテキスト
	 * @param outName 実在した場合のライト名
	 * @return 実在すれば真。平行光源は LightManager があれば常に真
	 */
	bool ResolveLight(const BindingContext& ctx, std::string& outName) const;

	LightTrackKind kind_ = LightTrackKind::Spot;

	// 全種類共通
	Vector4Curve colorCurve_;
	FloatCurve intensityCurve_;
	// スポットのみ: コーンの半角（ラジアン）。内部で cos に変換して渡す
	FloatCurve coneAngleCurve_;
	// ポイントのみ: 届く半径
	FloatCurve radiusCurve_;

	CurveChannel<Vector4> colorChannel_{ "Color", &colorCurve_ };
	CurveChannel<float> intensityChannel_{ "Intensity", &intensityCurve_ };
	CurveChannel<float> coneAngleChannel_{ "Cone Angle", &coneAngleCurve_ };
	CurveChannel<float> radiusChannel_{ "Radius", &radiusCurve_ };
	// スポットのみ: ビームの明るさの倍率。カーブを持つ間だけビームを出す
	FloatCurve beamCurve_;
	CurveChannel<float> beamChannel_{ "Beam", &beamCurve_ };

	// --- 状態の退避 ---
	bool hasCapturedState_ = false;
	Vector4 capturedColor_{};
	float capturedIntensity_ = 0.0f;
	float capturedCosAngle_ = 0.0f;
	float capturedRadius_ = 0.0f;
	bool capturedBeamEnabled_ = false;
	float capturedBeamScale_ = 1.0f;
};
} // namespace KCE
