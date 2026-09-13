#pragma once
#include <string>

#include "math/Quaternion.h"
#include "math/Vector3.h"
#include "sequencer/core/Curve.h"
#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief カメラの位置・回転・画角をカーブで駆動するトラック
 *
 * @details 回転はクォータニオンで保持し Slerp で補間する。オイラー角のまま
 *          補間するとカットをまたぐ大きな回転で必ず破綻するため（SEQUENCER_PLAN 7.4）。
 *
 *          注目点（Aim）を使うと、向きは回転のキーではなく「どこを見るか」から毎フレーム決める。
 *          役 A / B に割り当てた GameObject の位置を混ぜ具合で混ぜ、ずらし量を足した点を狙う。
 *          役が両方空なら、ずらし量をワールドの座標としてそのまま狙う。
 *
 *          揺れは2種類。拍の揺れはシーケンスの BPM から拍の時刻を求め、拍ごとに画角をはねさせる。
 *          手持ちの揺れは時刻から決まるなめらかな波で向きを振る。どちらも強さはキーで決める。
 *
 *          Evaluate() は ITrack の純関数契約を守る。内部に「前フレームの値」を
 *          持たず、時刻と割り当てた実体の今の位置だけから決めてカメラに書き込む。
 */
class CameraTrack : public ITrack
{
public:
	/** @brief 拍の揺れを起こす間隔 */
	enum class BeatDivision
	{
		EveryBeat,		//!< 毎拍
		EveryTwoBeats,	//!< 2拍ごと
		EveryBar,		//!< 小節の頭（4拍ごと。エディタの濃い線と同じ）
	};

	CameraTrack();

	TrackType GetType() const override { return TrackType::Camera; }
	const char* GetTypeName() const override { return "Camera"; }

	void Evaluate(float time, const BindingContext& ctx) override;
	float GetEndTime() const override;

	void CaptureState(const BindingContext& ctx) override;
	void RestoreState(const BindingContext& ctx) override;

	nlohmann::json Serialize() const override;
	bool Deserialize(const nlohmann::json& json) override;

	size_t GetChannelCount() const override { return kChannelCount; }
	ICurveChannel* GetChannel(size_t index) override;
	bool RecordKey(float time, const BindingContext& ctx) override;

#ifdef USE_IMGUI
	bool DrawInspector() override;
#endif

	// --- カーブへのアクセス ---

	Vector3Curve& GetPositionCurve() { return positionCurve_; }
	const Vector3Curve& GetPositionCurve() const { return positionCurve_; }

	QuaternionCurve& GetRotationCurve() { return rotationCurve_; }
	const QuaternionCurve& GetRotationCurve() const { return rotationCurve_; }

	FloatCurve& GetFovCurve() { return fovCurve_; }
	const FloatCurve& GetFovCurve() const { return fovCurve_; }

	// --- 注目点 ---

	/** @brief 注目点にする役 A（GameObject）。空なら使わない */
	const std::string& GetAimRoleA() const { return aimRoleA_; }
	/** @brief 注目点にする役 B（GameObject）。空なら使わない */
	const std::string& GetAimRoleB() const { return aimRoleB_; }

	/**
	 * @brief 注目点を使う設定になっているか
	 * @return 役か、ずらし量のキーがあれば真
	 */
	bool UsesAim() const;

private:
	/** @brief チャンネル数（位置・回転・画角・注目点のずらし量・混ぜ具合・ロール・拍の揺れ・手持ちの揺れ） */
	static constexpr size_t kChannelCount = 8;
	// 拍の揺れが戻るまでの時定数（秒）の初期値
	static constexpr float kDefaultBeatShakeDecay = 0.15f;
	// 手持ちの揺れの速さ（Hz）の初期値
	static constexpr float kDefaultHandheldFrequency = 0.5f;

	/**
	 * @brief 指定時刻の拍の揺れで、画角をどれだけ狭めるか
	 * @param time 時刻（秒）
	 * @param ctx BPM の入ったコンテキスト
	 * @return 狭める量（度）。BPM が 0 以下か、キーが無ければ 0
	 */
	float EvaluateBeatKick(float time, const BindingContext& ctx) const;

	/**
	 * @brief 指定時刻の注目点を求める
	 * @param time 時刻（秒）
	 * @param ctx 役の割り当て
	 * @param outTarget 注目点（ワールド）
	 * @return 求められたら真。役を指定しているのに割り当てが無ければ偽
	 */
	bool EvaluateAimTarget(float time, const BindingContext& ctx, Vector3& outTarget) const;

	Vector3Curve positionCurve_;
	QuaternionCurve rotationCurve_;
	// 垂直画角（ラジアン）
	FloatCurve fovCurve_;
	// 注目点のずらし量。役があればその位置から、無ければワールドの座標そのもの
	Vector3Curve aimOffsetCurve_;
	// 役 A と B の混ぜ具合（0 で A、1 で B）
	FloatCurve aimBlendCurve_;
	// 画面の傾き（度）。カメラの前方向を軸に回す
	FloatCurve rollCurve_;
	// 拍の揺れの強さ（度）。拍の瞬間に画角をこれだけ狭めて、すぐ戻す
	FloatCurve beatShakeCurve_;
	// 手持ちの揺れの強さ（度）。向きを時刻から決まるなめらかな波で振る
	FloatCurve handheldCurve_;

	// 注目点にする役（GameObject）
	std::string aimRoleA_;
	std::string aimRoleB_;

	// 拍の揺れを起こす間隔
	BeatDivision beatDivision_ = BeatDivision::EveryBeat;
	// 拍の揺れが戻るまでの時定数（秒）
	float beatShakeDecay_ = kDefaultBeatShakeDecay;
	// 手持ちの揺れの速さ（Hz）
	float handheldFrequency_ = kDefaultHandheldFrequency;

	// カーブをエディタへ見せる窓口。カーブの実体はこのクラスが持つ
	CurveChannel<Vector3> positionChannel_{ "Position", &positionCurve_ };
	CurveChannel<Quaternion> rotationChannel_{ "Rotation", &rotationCurve_ };
	CurveChannel<float> fovChannel_{ "FOV", &fovCurve_ };
	CurveChannel<Vector3> aimOffsetChannel_{ "Aim Offset", &aimOffsetCurve_ };
	CurveChannel<float> aimBlendChannel_{ "Aim Blend", &aimBlendCurve_ };
	CurveChannel<float> rollChannel_{ "Roll (deg)", &rollCurve_ };
	CurveChannel<float> beatShakeChannel_{ "Beat Shake (deg)", &beatShakeCurve_ };
	CurveChannel<float> handheldChannel_{ "Handheld (deg)", &handheldCurve_ };

	// --- 状態の退避（SEQUENCER_PLAN 3.5） ---
	bool hasCapturedState_ = false;
	Vector3 capturedPosition_{};
	Quaternion capturedRotation_ = Quaternion::Identity();
	float capturedFov_ = 0.0f;
	bool capturedUsedQuaternion_ = false;
	Vector3 capturedEuler_{};
};
} // namespace KCE
