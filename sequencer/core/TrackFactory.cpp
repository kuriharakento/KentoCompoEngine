#include "sequencer/core/TrackFactory.h"

#include "sequencer/track/CameraTrack.h"
#include "sequencer/track/ComponentTrack.h"
#include "sequencer/track/EventTrack.h"
#include "sequencer/track/LightTrack.h"
#include "sequencer/track/PostProcessTrack.h"
#include "sequencer/track/ScreenTrack.h"
#include "sequencer/track/Text3DTrack.h"
#include "sequencer/track/TextTrack.h"
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
	"Screen",
	"Text",
	"Text3D",
	"Component",
};
} // namespace

TrackPtr TrackFactory::Create(const std::string& typeName)
{
	if (typeName == "Camera") { return std::make_unique<CameraTrack>(); }
	if (typeName == "Transform") { return std::make_unique<TransformTrack>(); }
	if (typeName == "Light") { return std::make_unique<LightTrack>(); }
	if (typeName == "PostProcess") { return std::make_unique<PostProcessTrack>(); }
	if (typeName == "Event") { return std::make_unique<EventTrack>(); }
	if (typeName == "Screen") { return std::make_unique<ScreenTrack>(); }
	if (typeName == "Text") { return std::make_unique<TextTrack>(); }
	if (typeName == "Text3D") { return std::make_unique<Text3DTrack>(); }
	if (typeName == "Component") { return std::make_unique<ComponentTrack>(); }

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
	case TrackType::Screen:      return std::make_unique<ScreenTrack>();
	case TrackType::Text:        return std::make_unique<TextTrack>();
	case TrackType::Text3D:      return std::make_unique<Text3DTrack>();
	case TrackType::Component:   return std::make_unique<ComponentTrack>();
	default:                     return nullptr;
	}
}

const char* const* TrackFactory::GetCreatableTypeNames(size_t& outCount)
{
	outCount = sizeof(kCreatableTypeNames) / sizeof(kCreatableTypeNames[0]);
	return kCreatableTypeNames;
}
} // namespace KCE
