#include "sequencer/runtime/CutsceneManager.h"

#include <algorithm>

#include "base/Camera.h"
#include "base/Logger.h"
#include "input/Input.h"
#include "manager/scene/CameraManager.h"
#include "time/TimeManager.h"

#ifdef USE_IMGUI
#include <cstdio>
#include "externals/imgui/imgui.h"
#include "manager/editor/DebugUIManager.h"
#endif

namespace KCE
{
namespace
{
/** @brief カットシーン用カメラの名前 */
const char* const kCutsceneCameraName = "CutsceneCamera";
/** @brief シーケンスがカメラとして要求する役 */
const char* const kCameraRole = "MainCam";

/** @brief 始めと終わりをゆっくりにした 0〜1 の補間係数 */
float SmoothWeight(float elapsed, float duration)
{
	if (duration <= 0.0f)
	{
		return 1.0f;
	}
	const float t = std::clamp(elapsed / duration, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

/** @brief 2つの姿勢の間をカメラに適用する */
void ApplyBlendedPose(Camera* camera,
	const Vector3& fromPosition, const Quaternion& fromRotation, float fromFov,
	const Vector3& toPosition, const Quaternion& toRotation, float toFov, float weight)
{
	camera->SetTranslate(fromPosition + (toPosition - fromPosition) * weight);
	camera->SetRotateQuaternion(Quaternion::Slerp(fromRotation, toRotation, weight));
	camera->SetFovY(fromFov + (toFov - fromFov) * weight);
}
} // namespace

std::unique_ptr<CutsceneManager> CutsceneManager::instance_ = nullptr;

CutsceneManager* CutsceneManager::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<CutsceneManager>();
	}
	return instance_.get();
}

bool CutsceneManager::HasInstance()
{
	return instance_ != nullptr;
}

void CutsceneManager::Initialize(CameraManager* cameraManager, LightManager* lightManager, PostProcessManager* postProcessManager)
{
	cameraManager_ = cameraManager;
	if (cameraManager_)
	{
		cameraManager_->AddCamera(kCutsceneCameraName);
		cutsceneCamera_ = cameraManager_->GetCamera(kCutsceneCameraName);
	}

	player_.GetBindingContext().SetLightManager(lightManager);
	player_.GetBindingContext().SetPostProcessManager(postProcessManager);
	player_.SetEventCallback([this](const std::string& eventName)
	{
		if (request_.onEvent)
		{
			request_.onEvent(eventName);
		}
	});
}

void CutsceneManager::SetAtmosphere(FogRenderer* fogRenderer, BeamRenderer* beamRenderer)
{
	player_.GetBindingContext().SetFogRenderer(fogRenderer);
	player_.GetBindingContext().SetBeamRenderer(beamRenderer);
}

void CutsceneManager::SetTextOverlay(TextOverlay* textOverlay)
{
	player_.GetBindingContext().SetTextOverlay(textOverlay);
}

void CutsceneManager::Finalize()
{
#ifdef USE_IMGUI
	if (DebugUIManager::HasInstance())
	{
		DebugUIManager::GetInstance()->UnregisterDebugUI(this);
	}
#endif
	// ロックしたまま終わらないようにする
	if (state_ != State::Idle && request_.lockInput)
	{
		Input::GetInstance()->SetGameplayLocked(false);
	}
	player_.SetSequence(nullptr);
	instance_.reset();
}

Camera* CutsceneManager::GetGameplayCamera() const
{
	return cameraManager_ ? cameraManager_->GetCamera(previousCameraName_) : nullptr;
}

bool CutsceneManager::Play(const CutsceneRequest& request, std::string* outError)
{
	if (!cameraManager_ || !cutsceneCamera_)
	{
		if (outError) { *outError = "CutsceneManager が初期化されていません"; }
		return false;
	}

	// 再生中なら、今のカットシーンを中断してから始める
	if (state_ != State::Idle)
	{
		Stop();
	}

	player_.SetSequence(nullptr);
	std::string error;
	if (!sequence_.LoadFromFile(request.sequencePath, &error))
	{
		Logger::Log("CutsceneManager: 読み込みに失敗しました: " + error + "\n", Logger::LogLevel::Error);
		if (outError) { *outError = error; }
		return false;
	}

	request_ = request;
	skipped_ = false;

	// 役の割り当て。カメラの役はカットシーン用カメラに固定する
	BindingContext& ctx = player_.GetBindingContext();
	ctx.Clear();
	if (request_.bind)
	{
		request_.bind(ctx);
	}
	ctx.BindCamera(kCameraRole, cutsceneCamera_);

	SequenceOrigin origin;
	origin.enabled = request_.useOrigin;
	origin.position = request_.originPosition;
	origin.yaw = request_.originYaw;
	ctx.SetOrigin(origin);

	// ゲームのカメラの姿勢を覚え、カットシーン用カメラをそこから始める。
	// シーケンスにカメラのキーが無くても、画面が飛ばないようにするため
	previousCameraName_ = cameraManager_->GetActiveCameraName();
	if (Camera* gameplay = GetGameplayCamera())
	{
		blendFromPosition_ = gameplay->GetTranslate();
		blendFromRotation_ = gameplay->GetRotateQuaternion();
		blendFromFov_ = gameplay->GetFovY();
		cutsceneCamera_->SetTranslate(blendFromPosition_);
		cutsceneCamera_->SetRotateQuaternion(blendFromRotation_);
		cutsceneCamera_->SetFovY(blendFromFov_);
	}
	cameraManager_->SetActiveCamera(kCutsceneCameraName);

	if (request_.lockInput)
	{
		Input::GetInstance()->SetGameplayLocked(true);
	}

	player_.SetSequence(&sequence_);
	player_.Play();
	state_ = State::Playing;
	stateTime_ = 0.0f;
	return true;
}

void CutsceneManager::Skip()
{
	if (state_ != State::Playing)
	{
		return;
	}

	// 純関数契約のおかげで、末尾を1回評価するだけで最終状態になる。
	// 進行に必要なイベント（fireOnSkip）はこの中で発火する
	player_.SkipToEnd();
	skipped_ = true;
	BeginBlendOut();
}

void CutsceneManager::Stop()
{
	if (state_ == State::Idle)
	{
		return;
	}

	// 演出前の状態に戻して、ブレンドせずに終わる
	player_.Stop();
	skipped_ = true;
	Finish();
}

void CutsceneManager::BeginBlendOut()
{
	// 今の演出のカメラの姿勢から、ゲームのカメラへ戻っていく
	blendFromPosition_ = cutsceneCamera_->GetTranslate();
	blendFromRotation_ = cutsceneCamera_->GetRotateQuaternion();
	blendFromFov_ = cutsceneCamera_->GetFovY();
	state_ = State::BlendingOut;
	stateTime_ = 0.0f;

	if (request_.blendOutTime <= 0.0f)
	{
		Finish();
	}
}

void CutsceneManager::Finish()
{
	if (cameraManager_ && !previousCameraName_.empty())
	{
		cameraManager_->SetActiveCamera(previousCameraName_);
	}
	if (request_.lockInput)
	{
		Input::GetInstance()->SetGameplayLocked(false);
	}

	// 再生し切った・スキップした場合、プレイヤーは状態を復元しない（最終状態を残す）
	player_.SetSequence(nullptr);
	state_ = State::Idle;

	// 通知の中で次のカットシーンを始められるよう、状態を片付けてから呼ぶ
	auto onFinished = std::move(request_.onFinished);
	const bool skipped = skipped_;
	request_ = CutsceneRequest{};
	if (onFinished)
	{
		onFinished(skipped);
	}
}

void CutsceneManager::Update()
{
	if (state_ == State::Idle || !cutsceneCamera_)
	{
		return;
	}

	// 編集モードでゲーム時間が止まっていても、カットシーンの経過は実時間で測る。
	// ゲーム側の realDeltaTime は一時停止中に 0 になるので、止まらない UI 側の時間を使う
	const float deltaTime = TimeManager::GetInstance().GetUIContext().realDeltaTime;
	stateTime_ += deltaTime;

	if (state_ == State::Playing)
	{
		// スキップはロック中でも受け付けるので、ロックの影響を受けない入力を見る
		Input* input = Input::GetInstance();
		if (request_.skippable &&
			(input->TriggerKeyRaw(DIK_ESCAPE) || input->TriggerKeyRaw(DIK_RETURN) || input->IsButtonTriggeredRaw(0, XINPUT_GAMEPAD_START)))
		{
			Skip();
			return;
		}

		player_.Update();

		// 開始直後は、ゲームのカメラの姿勢から演出の姿勢へなめらかに移す。
		// トラックが書き込んだ姿勢を「行き先」として、その上に重ねる
		if (stateTime_ < request_.blendInTime)
		{
			const float weight = SmoothWeight(stateTime_, request_.blendInTime);
			ApplyBlendedPose(cutsceneCamera_,
				blendFromPosition_, blendFromRotation_, blendFromFov_,
				cutsceneCamera_->GetTranslate(), cutsceneCamera_->GetRotateQuaternion(), cutsceneCamera_->GetFovY(),
				weight);
		}

		// 末尾まで再生し切った
		if (player_.GetState() == PlaybackState::Stopped)
		{
			BeginBlendOut();
		}
		return;
	}

	if (state_ == State::BlendingOut)
	{
		// ゲームのカメラは戻る間も動いているかもしれないので、毎フレーム行き先を取り直す
		Camera* gameplay = GetGameplayCamera();
		if (!gameplay)
		{
			Finish();
			return;
		}

		const float weight = SmoothWeight(stateTime_, request_.blendOutTime);
		ApplyBlendedPose(cutsceneCamera_,
			blendFromPosition_, blendFromRotation_, blendFromFov_,
			gameplay->GetTranslate(), gameplay->GetRotateQuaternion(), gameplay->GetFovY(),
			weight);

		if (stateTime_ >= request_.blendOutTime)
		{
			Finish();
		}
	}
}

#ifdef USE_IMGUI
void CutsceneManager::RegisterDebugUI()
{
	DebugUIManager::GetInstance()->RegisterDebugUI(this, "Cutscene", [this]() { DrawImGui(); }, DebugUIArea::Inspector);
}

void CutsceneManager::DrawImGui()
{
	ImGui::TextDisabled("ゲームからの再生（CutsceneManager::Play）を試すためのUI");

	char pathBuffer[256];
	std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", debugPath_.c_str());
	if (ImGui::InputText("Sequence", pathBuffer, sizeof(pathBuffer)))
	{
		debugPath_ = pathBuffer;
	}

	static const char* const kStateNames[] = { "Idle", "Playing", "BlendingOut" };
	ImGui::Text("State: %s  Time: %.2f / %.2f", kStateNames[static_cast<int>(state_)], player_.GetTime(), player_.GetDuration());

	if (ImGui::Button("Play"))
	{
		CutsceneRequest request;
		request.sequencePath = debugPath_;
		request.onEvent = [this](const std::string& eventName)
		{
			debugMessage_ = "Event: " + eventName;
			Logger::Log("Cutscene Event: " + eventName + "\n");
		};
		request.onFinished = [this](bool skipped)
		{
			debugMessage_ = skipped ? "終了（スキップ／中断）" : "終了（最後まで再生）";
		};
		std::string error;
		if (!Play(request, &error))
		{
			debugMessage_ = "再生できません: " + error;
		}
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(state_ != State::Playing);
	if (ImGui::Button("Skip")) { Skip(); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(state_ == State::Idle);
	if (ImGui::Button("Stop")) { Stop(); }
	ImGui::EndDisabled();

	if (!debugMessage_.empty())
	{
		ImGui::TextDisabled("%s", debugMessage_.c_str());
	}
}
#endif
} // namespace KCE
