#include "editor/SceneViewContext.h"

namespace KCE
{
std::unique_ptr<SceneViewContext> SceneViewContext::instance_ = nullptr;

SceneViewContext* SceneViewContext::GetInstance()
{
	if (!instance_)
	{
		instance_ = std::make_unique<SceneViewContext>();
	}
	return instance_.get();
}

bool SceneViewContext::HasInstance()
{
	return instance_ != nullptr;
}

void SceneViewContext::Finalize()
{
	camera_ = nullptr;
	instance_.reset();
}
} // namespace KCE
