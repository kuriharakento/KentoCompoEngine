#include "sequencer/core/TrackFactory.h"

#include "sequencer/track/CameraTrack.h"
#include "sequencer/track/EventTrack.h"
#include "sequencer/track/LightTrack.h"
#include "sequencer/track/PostProcessTrack.h"
#include "sequencer/track/TransformTrack.h"

namespace KCE
{
namespace
{
/** @brief エディタから追加できるトラック種別 */
const char* const kCreatableTypeNames[] = {
	"Camera",
	"Transform",
	"Light",
	"PostProcess",
	"Event",
};
} // namespace

TrackPtr TrackFactory::Create(const std::string& typeName)
{
	if (typeName == "Camera") { return std::make_unique<CameraTrack>(); }
	if (typeName == "Transform") { return std::make_unique<TransformTrack>(); }
	if (typeName == "Light") { return std::make_unique<LightTrack>(); }
	if (typeName == "PostProcess") { return std::make_unique<PostProcessTrack>(); }
	if (typeName == "Event") { return std::make_unique<EventTrack>(); }

	// 未知の種別。新しいバージョンで保存されたデータを開いた場合に起きる。
	// 落とさずに nullptr を返し、呼び出し側で読み飛ばす。
	return nullptr;
}

TrackPtr TrackFactory::Create(TrackType type)
{
	switch (type)
	{
	case TrackType::Camera:      return std::make_unique<CameraTrack>();
	case TrackType::Transform:   return std::make_unique<TransformTrack>();
	case TrackType::Light:       return std::make_unique<LightTrack>();
	case TrackType::PostProcess: return std::make_unique<PostProcessTrack>();
	case TrackType::Event:       return std::make_unique<EventTrack>();
	default:                     return nullptr;
	}
}

const char* const* TrackFactory::GetCreatableTypeNames(size_t& outCount)
{
	outCount = sizeof(kCreatableTypeNames) / sizeof(kCreatableTypeNames[0]);
	return kCreatableTypeNames;
}
} // namespace KCE
