#pragma once
#include <string>

#include "sequencer/core/ITrack.h"

namespace KCE
{
/**
 * @brief 種別名からトラックを生成するファクトリ
 * @details JSONに保存されているのは列挙値ではなく種別名なので、
 *          読み込み時はここを通して実体を作る。
 */
class TrackFactory
{
public:
	/**
	 * @brief 種別名からトラックを生成する
	 * @param typeName 種別名（例: "Camera"）
	 * @return 生成されたトラック。未知の種別なら nullptr
	 */
	static TrackPtr Create(const std::string& typeName);

	/**
	 * @brief 種別からトラックを生成する
	 * @param type 種別
	 * @return 生成されたトラック。未知の種別なら nullptr
	 */
	static TrackPtr Create(TrackType type);

	/**
	 * @brief エディタの「トラックを追加」に出す種別の一覧
	 * @return 種別名の配列と、その要素数
	 */
	static const char* const* GetCreatableTypeNames(size_t& outCount);
};
} // namespace KCE
