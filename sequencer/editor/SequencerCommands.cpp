#include "sequencer/editor/SequencerCommands.h"

#include <utility>

#include "base/Logger.h"

namespace KCE
{
namespace
{
/**
 * @brief トラックのスナップショットを取る
 * @param sequence 対象シーケンス
 * @param trackIndex トラックのインデックス
 * @return トラックのJSON。トラックが無ければ空のJSON
 */
nlohmann::json SnapshotTrack(Sequence* sequence, size_t trackIndex)
{
	if (!sequence)
	{
		return nlohmann::json();
	}
	ITrack* track = sequence->GetTrack(trackIndex);
	return track ? track->Serialize() : nlohmann::json();
}

/**
 * @brief スナップショットをトラックへ流し込む
 * @param sequence 対象シーケンス
 * @param trackIndex トラックのインデックス
 * @param snapshot 流し込むJSON
 */
void RestoreTrack(Sequence* sequence, size_t trackIndex, const nlohmann::json& snapshot)
{
	if (!sequence || snapshot.is_null())
	{
		return;
	}

	ITrack* track = sequence->GetTrack(trackIndex);
	if (!track)
	{
		// Undo/Redo の順序が壊れているとここに来る。落とさずログに出す。
		Logger::Log("TrackEditCommand: 対象トラックが見つかりません\n", Logger::LogLevel::Warning);
		return;
	}

	track->Deserialize(snapshot);
}
} // namespace

TrackEditCommand::TrackEditCommand(Sequence* sequence, size_t trackIndex, std::string name)
	: sequence_(sequence)
	, trackIndex_(trackIndex)
	, name_(std::move(name))
	, before_(SnapshotTrack(sequence, trackIndex))
{
}

void TrackEditCommand::CaptureAfter()
{
	after_ = SnapshotTrack(sequence_, trackIndex_);
	hasAfter_ = true;
}

bool TrackEditCommand::HasChanged() const
{
	return hasAfter_ && before_ != after_;
}

void TrackEditCommand::Execute()
{
	// 呼び出し側が既にトラックを編集済みの状態で履歴へ積むため、
	// 初回の Execute では何もしない。Redo のときだけ編集後の状態を流し込む。
	if (hasAfter_)
	{
		RestoreTrack(sequence_, trackIndex_, after_);
	}
}

void TrackEditCommand::Undo()
{
	RestoreTrack(sequence_, trackIndex_, before_);
}

bool TrackEditCommand::MergeWith(const ICommand* next)
{
	const auto* other = dynamic_cast<const TrackEditCommand*>(next);
	if (!other || other->sequence_ != sequence_ || other->trackIndex_ != trackIndex_ || other->name_ != name_)
	{
		return false;
	}

	// 開始状態は自分のものを保ったまま、終了状態だけ引き継ぐ
	after_ = other->after_;
	hasAfter_ = other->hasAfter_;
	return true;
}

SequenceStructureCommand::SequenceStructureCommand(Sequence* sequence, std::string name)
	: sequence_(sequence)
	, name_(std::move(name))
	, before_(sequence ? sequence->Serialize() : nlohmann::json())
{
}

void SequenceStructureCommand::CaptureAfter()
{
	after_ = sequence_ ? sequence_->Serialize() : nlohmann::json();
	hasAfter_ = true;
}

bool SequenceStructureCommand::HasChanged() const
{
	return hasAfter_ && before_ != after_;
}

void SequenceStructureCommand::Execute()
{
	if (hasAfter_ && sequence_)
	{
		sequence_->Deserialize(after_);
	}
}

void SequenceStructureCommand::Undo()
{
	if (sequence_ && !before_.is_null())
	{
		sequence_->Deserialize(before_);
	}
}
} // namespace KCE
