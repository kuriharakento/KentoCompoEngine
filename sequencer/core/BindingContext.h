#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "math/Quaternion.h"
#include "math/Vector3.h"

namespace KCE
{
class Camera;
class GameObject;
class BeamRenderer;
class FogRenderer;
class LightManager;
class PostProcessManager;
class TextOverlay;
class Text3DRenderer;
class DepthOfFieldRenderer;

/**
 * @brief 役（ロール）に割り当てられる実体の型
 */
enum class BindingType
{
	GameObject, //!< シーン上のオブジェクト
	Camera,		//!< カメラ
	Light,		//!< ライト（名前で参照する）
};

/**
 * @brief シーケンスが要求する役の定義
 * @details シーケンス自体はオブジェクトを直接指さず、この役だけを持つ。
 *          「攻撃演出」「会話」のような演出を部品として使い回すために必要。
 */
struct BindingDefinition
{
	std::string role;							  //!< 役の名前（例: "Target", "MainCam"）
	BindingType type = BindingType::GameObject;	  //!< 割り当てられる実体の型
	std::string description;					  //!< エディタに表示する説明（任意）
};

/**
 * @brief シーケンスを置く原点
 *
 * @details SEQUENCER_PLAN 7.7「現在地を原点とする相対座標シーケンス」。
 *          「被弾リアクション」のカメラワークを原点まわりで作っておけば、
 *          どこにいる敵に対しても、その位置と向きに合わせて再生できる。
 *          向きは水平（Y 軸回り）のみ。演出で傾いた原点が要る場面はまず無いため。
 */
struct SequenceOrigin
{
	bool enabled = false;
	// 原点の位置（ワールド）
	Vector3 position{};
	// 原点の向き（Y 軸回り、ラジアン）
	float yaw = 0.0f;
};

/**
 * @brief 役に実体を割り当てた、再生時のコンテキスト
 *
 * @details SEQUENCER_PLAN 3.2。Play(seq, { {"Target", enemyA} }) のように、
 *          再生のたびに違う実体を差し込めるようにする。
 *          トラックは Evaluate() の中でここから実体を引く。
 *
 *          役に何も割り当てられていない場合は nullptr が返るため、
 *          トラック側は必ず null チェックしてから触ること。
 *          エディタでプレビュー中に対象を外していることは日常的に起きる。
 */
class BindingContext
{
public:
	/**
	 * @brief 役にGameObjectを割り当てる
	 * @param role 役の名前
	 * @param gameObject 割り当てる実体。nullptrで割り当て解除
	 */
	void BindGameObject(const std::string& role, GameObject* gameObject);

	/**
	 * @brief 役にカメラを割り当てる
	 * @param role 役の名前
	 * @param camera 割り当てる実体。nullptrで割り当て解除
	 */
	void BindCamera(const std::string& role, Camera* camera);

	/**
	 * @brief 役にライト名を割り当てる
	 * @details ライトは LightManager が名前で管理しているため、実体ではなく名前を持つ。
	 * @param role 役の名前
	 * @param lightName ライト名
	 */
	void BindLight(const std::string& role, const std::string& lightName);

	/**
	 * @brief 役に割り当てられたGameObjectを取得する
	 * @param role 役の名前
	 * @return 実体。未割り当てなら nullptr
	 */
	GameObject* GetGameObject(const std::string& role) const;

	/**
	 * @brief 役に割り当てられたカメラを取得する
	 * @param role 役の名前
	 * @return 実体。未割り当てなら nullptr
	 */
	Camera* GetCamera(const std::string& role) const;

	/**
	 * @brief 役に割り当てられたライト名を取得する
	 * @param role 役の名前
	 * @return ライト名。未割り当てなら空文字列
	 */
	const std::string& GetLightName(const std::string& role) const;

	// --- 役を持たない共有システム ---
	// ライトは名前で、ポストプロセスは画面全体で1つなので、
	// 実体へ届くためのシステムもコンテキスト経由で渡す。

	void SetLightManager(LightManager* lightManager) { lightManager_ = lightManager; }
	LightManager* GetLightManager() const { return lightManager_; }

	void SetPostProcessManager(PostProcessManager* postProcessManager) { postProcessManager_ = postProcessManager; }
	PostProcessManager* GetPostProcessManager() const { return postProcessManager_; }

	void SetFogRenderer(FogRenderer* fogRenderer) { fogRenderer_ = fogRenderer; }
	FogRenderer* GetFogRenderer() const { return fogRenderer_; }

	void SetBeamRenderer(BeamRenderer* beamRenderer) { beamRenderer_ = beamRenderer; }
	BeamRenderer* GetBeamRenderer() const { return beamRenderer_; }

	// 歌詞と会話の出力先。画面に1つ
	void SetTextOverlay(TextOverlay* textOverlay) { textOverlay_ = textOverlay; }
	TextOverlay* GetTextOverlay() const { return textOverlay_; }

	// 3D 空間の文字。Text3D トラックはここから名前で探す
	void SetText3DRenderer(Text3DRenderer* text3DRenderer) { text3DRenderer_ = text3DRenderer; }
	Text3DRenderer* GetText3DRenderer() const { return text3DRenderer_; }

	// 被写界深度。カメラトラックがピントを注目点に合わせるときに使う。画面に1つ
	void SetDepthOfFieldRenderer(DepthOfFieldRenderer* depthOfFieldRenderer) { depthOfFieldRenderer_ = depthOfFieldRenderer; }
	DepthOfFieldRenderer* GetDepthOfFieldRenderer() const { return depthOfFieldRenderer_; }

	// --- 拍 ---

	/**
	 * @brief 拍の刻みを設定する
	 * @details シーケンスのメタ情報を、再生側（SequencePlayer）が評価の直前に入れる。
	 *          拍に合わせて動くトラック（カメラの拍の揺れなど）が使う。
	 * @param bpm 1分あたりの拍数。0 以下なら拍を使わない
	 * @param offset 1拍目の時刻（秒、シーケンス先頭から）。エディタの拍の線と同じ基準
	 */
	void SetBeatTiming(float bpm, float offset) { bpm_ = bpm; beatOffset_ = offset; }
	float GetBpm() const { return bpm_; }
	float GetBeatOffset() const { return beatOffset_; }

	// --- 原点 ---

	/**
	 * @brief シーケンスを置く原点を設定する
	 * @details エディタでは常に無効（ワールド座標のまま）で作る。
	 */
	void SetOrigin(const SequenceOrigin& origin) { origin_ = origin; }
	const SequenceOrigin& GetOrigin() const { return origin_; }

	/**
	 * @brief シーケンス内の位置を、原点を適用したワールドの位置へ変換する
	 * @param localPosition シーケンス内の位置
	 * @return ワールドの位置。原点が無効ならそのまま
	 */
	Vector3 ApplyOriginToPoint(const Vector3& localPosition) const;

	/**
	 * @brief シーケンス内の回転を、原点の向きを適用したワールドの回転へ変換する
	 * @param localRotation シーケンス内の回転
	 * @return ワールドの回転。原点が無効ならそのまま
	 */
	Quaternion ApplyOriginToRotation(const Quaternion& localRotation) const;

	/**
	 * @brief 役の割り当てを全て解除する
	 * @details 共有システム（LightManager / PostProcessManager）は解除しない。
	 */
	void Clear();

	/**
	 * @brief 何も割り当てられていないか
	 * @return 空なら真
	 */
	bool IsEmpty() const;

private:
	std::unordered_map<std::string, GameObject*> gameObjects_;
	std::unordered_map<std::string, Camera*> cameras_;
	std::unordered_map<std::string, std::string> lightNames_;
	LightManager* lightManager_ = nullptr;
	PostProcessManager* postProcessManager_ = nullptr;
	FogRenderer* fogRenderer_ = nullptr;
	BeamRenderer* beamRenderer_ = nullptr;
	TextOverlay* textOverlay_ = nullptr;
	Text3DRenderer* text3DRenderer_ = nullptr;
	DepthOfFieldRenderer* depthOfFieldRenderer_ = nullptr;
	SequenceOrigin origin_;
	// 拍の刻み。SequencePlayer が評価の直前に入れる
	float bpm_ = 0.0f;
	float beatOffset_ = 0.0f;
};
} // namespace KCE
