#include "sequencer/core/TrackFactory.h"

#include "sequencer/track/CameraTrack.h"

namespace KCE
{
namespace
{
/** @brief エディタから追加できるトラック種別 */
const char* const kCreatableTypeNames[] = {
	"Camera",
};
} // namespace

TrackPtr TrackFactory::Create(const std::string& typeName)
{
	if (typeName == "Camera")
	{
		return std::make_unique<CameraTrack>();
	}

	// 未知の種別。新しいバージョンで保存されたデータを開いた場合に起きる。
	// 落とさずに nullptr を返し、呼び出し側で読み飛ばす。
	return nullptr;
}

TrackPtr TrackFactory::Create(TrackType type)
{
	switch (type)
	{
	case TrackType::Camera: return std::make_unique<CameraTrack>();
	default:                return nullptr;
	}
}

const char* const* TrackFactory::GetCreatableTypeNames(size_t& outCount)
{
	outCount = sizeof(kCreatableTypeNames) / sizeof(kCreatableTypeNames[0]);
	return kCreatableTypeNames;
}
} // namespace KCE
