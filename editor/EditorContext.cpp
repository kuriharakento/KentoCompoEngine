#include "editor/EditorContext.h"

#include "time/TimeManager.h"

namespace KCE
{
std::unique_ptr<EditorContext> EditorContext::instance_ = nullptr;

EditorContext* EditorContext::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<EditorContext>();
	}
	return instance_.get();
}

bool EditorContext::HasInstance()
{
	return instance_ != nullptr;
}

void EditorContext::Finalize()
{
	// 止めたままプロセスを終えると、次に動かしたとき止まったままになりかねない
	if (mode_ == EditorMode::Edit)
	{
		TimeManager::GetInstance().Resume();
	}
	instance_.reset();
}

void EditorContext::SetMode(EditorMode mode)
{
	if (mode_ == mode)
	{
		return;
	}

	mode_ = mode;
	stepFrameRequested_ = false;

	if (mode_ == EditorMode::Edit)
	{
		TimeManager::GetInstance().Pause();
	}
	else
	{
		TimeManager::GetInstance().Resume();
	}
}

void EditorContext::ToggleMode()
{
	SetMode(mode_ == EditorMode::Edit ? EditorMode::Play : EditorMode::Edit);
}

bool EditorContext::ConsumeStepFrameRequest()
{
	const bool requested = stepFrameRequested_;
	stepFrameRequested_ = false;
	return requested;
}

bool EditorContext::ShouldUpdateGameLogic() const
{
	return mode_ == EditorMode::Play || stepFrameRequested_;
}
} // namespace KCE
