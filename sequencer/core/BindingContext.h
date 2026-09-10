#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace KCE
{
class Camera;
class GameObject;
class BeamRenderer;
class FogRenderer;
class LightManager;
class PostProcessManager;

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
};
} // namespace KCE
