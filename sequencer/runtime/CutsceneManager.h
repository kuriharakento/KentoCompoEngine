#pragma once
#include <functional>
#include <memory>
#include <string>

#include "math/Quaternion.h"
#include "math/Vector3.h"
#include "sequencer/core/Sequence.h"
#include "sequencer/core/SequencePlayer.h"

namespace KCE
{
class BeamRenderer;
class Camera;
class CameraManager;
class FogRenderer;
class LightManager;
class PostProcessManager;

/**
 * @brief カットシーンの再生要求
 *
 * @code
 * CutsceneRequest request;
 * request.sequencePath = "hit_reaction.json";
 * request.bind = [&](BindingContext& ctx) { ctx.BindGameObject("Target", enemy); };
 * request.useOrigin = true;                       // 敵の位置と向きを原点にする
 * request.originPosition = enemy->GetPosition();
 * request.originYaw = enemy->GetRotation().y;
 * request.onEvent = [&](const std::string& name) { if (name == "Damage") enemy->Damage(); };
 * request.onFinished = [&](bool skipped) { state = State::Battle; };
 * CutsceneManager::GetInstance()->Play(request);
 * @endcode
 */
struct CutsceneRequest
{
	// 再生するシーケンスのファイル（application/Resources/json/sequence からの相対、または絶対）
	std::string sequencePath;

	/**
	 * @brief 役に実体を割り当てる処理
	 * @details カメラの役 "MainCam" はカットシーン用のカメラに自動で割り当てるので、割り当て不要。
	 */
	std::function<void(BindingContext&)> bind;

	// ゲームのカメラから演出のカメラへ移る時間（秒）。0 で即座に切り替える
	float blendInTime = 0.5f;
	// 演出のカメラからゲームのカメラへ戻る時間（秒）。0 で即座に戻る
	float blendOutTime = 0.5f;

	// 再生中はゲームの操作を止めるか
	bool lockInput = true;
	// スキップできるか（Esc / Enter / ゲームパッドの START）
	bool skippable = true;

	// 現在地を原点にして再生するか。「被弾リアクション」のように、
	// 原点まわりで作った演出をどこにいる対象にも使い回すときに有効にする
	bool useOrigin = false;
	Vector3 originPosition{};
	// 原点の向き（Y 軸回り、ラジアン）
	float originYaw = 0.0f;

	// EventTrack のイベントを受け取る
	std::function<void(const std::string& eventName)> onEvent;
	// 終了時に呼ばれる。スキップ・中断で終わった場合は引数が真
	std::function<void(bool skipped)> onFinished;
};

/**
 * @brief ゲームの中でカットシーンを再生する窓口
 *
 * @details SEQUENCER_PLAN 7.7「ゲーム本編との接続」。エディタのシーケンサとは別に
 *          自前のプレイヤーを持ち、ゲームのシーンから呼べるようにする。
 *
 *          - ゲームのカメラと演出のカメラを、開始時と終了時になめらかにつなぐ
 *          - 再生中はゲームの操作を止め、スキップの入力だけを受け付ける
 *          - スキップは純関数契約により「末尾の時刻を1回評価する」だけで最終状態になる
 *
 *          カメラのブレンドはトラックの評価結果に後から重ねる表示上の処理であり、
 *          トラックの純関数契約には影響しない。
 */
class CutsceneManager
{
public:
	/** @brief 再生の段階 */
	enum class State
	{
		Idle,		 //!< 何も再生していない
		Playing,	 //!< 再生中（開始時のブレンドを含む）
		BlendingOut, //!< 終わってゲームのカメラへ戻っている途中
	};

	static CutsceneManager* GetInstance();
	static bool HasInstance();

	/**
	 * @brief 初期化する
	 * @details カットシーン用のカメラを CameraManager に追加する。
	 */
	void Initialize(CameraManager* cameraManager, LightManager* lightManager, PostProcessManager* postProcessManager);

	/** @brief トラックが駆動する大気（フォグとビーム）を設定する */
	void SetAtmosphere(FogRenderer* fogRenderer, BeamRenderer* beamRenderer);

	void Finalize();

	/**
	 * @brief カットシーンを再生する
	 * @details 再生中に呼んだ場合は、今のカットシーンを中断してから始める。
	 * @param request 再生要求
	 * @param outError 失敗理由の出力先（任意）
	 * @return 読み込みに成功して再生を始めたら真
	 */
	bool Play(const CutsceneRequest& request, std::string* outError = nullptr);

	/**
	 * @brief スキップする
	 * @details 最終状態を適用し、進行に必要なイベントだけを発火してから、ゲームのカメラへ戻る。
	 */
	void Skip();

	/**
	 * @brief 中断する
	 * @details 演出前の状態に戻し、ブレンドせずに即座に終わる。
	 */
	void Stop();

	/** @brief 毎フレームの更新。Framework::Update から呼ぶ */
	void Update();

	bool IsPlaying() const { return state_ != State::Idle; }
	State GetState() const { return state_; }

#ifdef USE_IMGUI
	void RegisterDebugUI();
	void DrawImGui();
#endif

public:
	~CutsceneManager() = default;

private:
	static std::unique_ptr<CutsceneManager> instance_;
	friend std::unique_ptr<CutsceneManager> std::make_unique<CutsceneManager>();

	CutsceneManager() = default;
	CutsceneManager(const CutsceneManager&) = delete;
	CutsceneManager& operator=(const CutsceneManager&) = delete;

	/** @brief ゲームのカメラへ戻る段階に入る */
	void BeginBlendOut();
	/** @brief 後片付けをして終了を通知する */
	void Finish();
	/** @brief ゲームのカメラ（カットシーン前にアクティブだったもの） */
	Camera* GetGameplayCamera() const;

	CameraManager* cameraManager_ = nullptr;
	Camera* cutsceneCamera_ = nullptr;
	std::string previousCameraName_;

	Sequence sequence_;
	SequencePlayer player_;
	CutsceneRequest request_;
	State state_ = State::Idle;
	bool skipped_ = false;

	// 段階に入ってからの経過時間（秒、実時間）
	float stateTime_ = 0.0f;

	// ブレンドの起点となるカメラの姿勢
	Vector3 blendFromPosition_{};
	Quaternion blendFromRotation_ = Quaternion::Identity();
	float blendFromFov_ = 0.45f;

#ifdef USE_IMGUI
	// デバッグUIで再生するファイル
	std::string debugPath_ = "sequence.json";
	std::string debugMessage_;
#endif
};
} // namespace KCE
