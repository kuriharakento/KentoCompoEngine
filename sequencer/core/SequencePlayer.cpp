#include "sequencer/core/SequencePlayer.h"

#include <algorithm>
#include <cmath>

#include "audio/Audio.h"
#include "time/TimeManager.h"

namespace KCE
{
void SequencePlayer::SetSequence(Sequence* sequence)
{
	if (sequence_ == sequence)
	{
		return;
	}

	// 対象を差し替える前に、今の対象を元の状態へ戻す
	Stop();
	sequence_ = sequence;
	time_ = 0.0f;
}

void SequencePlayer::Play()
{
	if (!sequence_)
	{
		return;
	}

	// 二重に退避すると「演出後の状態」を元の状態として覚えてしまう
	if (!hasCapturedState_)
	{
		sequence_->CaptureState(bindingContext_);
		hasCapturedState_ = true;
	}

	time_ = 0.0f;
	state_ = PlaybackState::Playing;
	StartAudio(0.0f);
	ApplyTime(time_);
}

void SequencePlayer::Resume()
{
	if (!sequence_ || state_ == PlaybackState::Playing)
	{
		return;
	}

	if (!hasCapturedState_)
	{
		sequence_->CaptureState(bindingContext_);
		hasCapturedState_ = true;
	}

	state_ = PlaybackState::Playing;
	StartAudio(time_);
}

void SequencePlayer::Pause()
{
	if (state_ != PlaybackState::Playing)
	{
		return;
	}

	state_ = PlaybackState::Paused;

	// 音声も止める。止めないと絵だけ静止して音が進み、再開時に同期がずれる
	if (audioStarted_ && sequence_ && !sequence_->GetMeta().audioClip.empty())
	{
		Audio::GetInstance()->Pause(sequence_->GetMeta().audioClip);
	}
}

void SequencePlayer::Stop()
{
	StopAudio();

	if (sequence_ && hasCapturedState_)
	{
		sequence_->RestoreState(bindingContext_);
	}

	hasCapturedState_ = false;
	state_ = PlaybackState::Stopped;
	time_ = 0.0f;
}

void SequencePlayer::Seek(float time)
{
	if (!sequence_)
	{
		return;
	}

	// スクラブ中も対象を書き換えるので、元の状態は必ず先に退避しておく
	if (!hasCapturedState_)
	{
		sequence_->CaptureState(bindingContext_);
		hasCapturedState_ = true;
	}

	const float duration = GetDuration();
	const float clamped = std::clamp(time, 0.0f, duration > 0.0f ? duration : 0.0f);

	// 音声が権威なので、音声側も同じ位置へ動かす
	if (audioStarted_ && !sequence_->GetMeta().audioClip.empty())
	{
		Audio::GetInstance()->Seek(sequence_->GetMeta().audioClip, clamped + sequence_->GetMeta().offset);
	}

	ApplyTime(clamped);
}

void SequencePlayer::SkipToEnd()
{
	if (!sequence_)
	{
		return;
	}

	// 純関数契約のおかげで、末尾を一度評価するだけで最終状態になる
	Seek(GetDuration());
	state_ = PlaybackState::Stopped;
	StopAudio();

	// スキップ後は演出の最終状態を残す。ここで RestoreState を呼ぶと
	// 「スキップしたら演出前に戻った」という誤った挙動になる。
	hasCapturedState_ = false;
}

void SequencePlayer::Update()
{
	if (!sequence_ || state_ != PlaybackState::Playing)
	{
		return;
	}

	const float duration = GetDuration();
	float newTime = time_;

	if (IsAudioDriven())
	{
		// 音声の再生位置を時間の権威とする。
		// deltaTime の積算では数分の楽曲で必ずズレる（SEQUENCER_PLAN 3.8）。
		const float audioPosition = Audio::GetInstance()->GetPlayPosition(sequence_->GetMeta().audioClip);
		newTime = audioPosition - sequence_->GetMeta().offset;
	}
	else
	{
		// 音声が無い場合は実時間で進める。
		// ゲームのタイムスケールに引きずられると、スロー演出中に
		// シーケンサ自身まで遅くなってしまう。
		newTime += TimeManager::GetInstance().GetGameContext().realDeltaTime;
	}

	if (duration > 0.0f && newTime >= duration)
	{
		if (loop_)
		{
			// 音声駆動の場合、音声側もループしているので位置を折り返す
			newTime = IsAudioDriven() ? std::fmod(newTime, duration) : 0.0f;
			if (!IsAudioDriven())
			{
				StartAudio(0.0f);
			}
		}
		else
		{
			newTime = duration;
			state_ = PlaybackState::Stopped;
			StopAudio();
			// 再生し切った場合は最終状態を残す
			hasCapturedState_ = false;
		}
	}

	ApplyTime((std::max)(newTime, 0.0f));
}

void SequencePlayer::EvaluateCurrentTime()
{
	ApplyTime(time_);
}

bool SequencePlayer::IsAudioDriven() const
{
	if (!sequence_ || sequence_->GetMeta().audioClip.empty() || !audioStarted_)
	{
		return false;
	}
	return Audio::GetInstance()->IsPlaying(sequence_->GetMeta().audioClip);
}

float SequencePlayer::GetDuration() const
{
	return sequence_ ? sequence_->GetDuration() : 0.0f;
}

void SequencePlayer::ApplyTime(float time)
{
	time_ = time;
	if (sequence_)
	{
		sequence_->Evaluate(time_, bindingContext_);
	}
}

void SequencePlayer::StartAudio(float startTime)
{
	if (!sequence_)
	{
		return;
	}

	const std::string& clip = sequence_->GetMeta().audioClip;
	if (clip.empty())
	{
		return;
	}

	Audio* audio = Audio::GetInstance();
	if (!audio->IsLoaded(clip))
	{
		// 読み込まれていない音声は無視して、実時間駆動にフォールバックする。
		// エディタでは音声ファイルが差し替えられていることが日常的に起きる。
		return;
	}

	if (audio->IsPaused(clip))
	{
		audio->Resume(clip);
	}
	else
	{
		audio->PlayWave(clip, loop_);
	}

	audio->Seek(clip, startTime + sequence_->GetMeta().offset);
	audioStarted_ = true;
}

void SequencePlayer::StopAudio()
{
	if (!audioStarted_ || !sequence_)
	{
		audioStarted_ = false;
		return;
	}

	const std::string& clip = sequence_->GetMeta().audioClip;
	if (!clip.empty())
	{
		Audio::GetInstance()->StopWave(clip);
	}
	audioStarted_ = false;
}
} // namespace KCE
