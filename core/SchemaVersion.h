#pragma once
#include <cstdint>

namespace KCE
{
/**
 * @brief JSONスキーマのバージョン定数
 * @details SEQUENCER_PLAN 3.4 の方針。エンジンが書き出す全てのJSONは先頭に
 *          "version" を持つ。フォーマットを非互換に変更するときは対応する定数を上げ、
 *          読み込み側で旧バージョンからのマイグレーションを分岐させること。
 *          バージョンを持たないJSONは「バージョン1相当の旧データ」として扱う。
 */

/** @brief GameObject のシリアライズ形式のバージョン */
constexpr int kGameObjectSchemaVersion = 1;

/** @brief Sequence（演出データ）のシリアライズ形式のバージョン */
constexpr int kSequenceSchemaVersion = 1;

/** @brief ステージファイルの現在のスキーマバージョン */
constexpr int kStageSchemaVersion = 1;

/**
 * @brief JSONに書かれていたバージョンが読み込み可能かどうか
 * @param loadedVersion 読み込んだJSONの version 値
 * @param currentVersion 現在のスキーマバージョン
 * @return 読み込めるなら真。未来のバージョンなら偽
 */
constexpr bool IsLoadableSchemaVersion(int loadedVersion, int currentVersion)
{
	// 未来のバージョンは解釈できないため拒否する。過去のバージョンは呼び出し側で移行する。
	return loadedVersion >= 1 && loadedVersion <= currentVersion;
}
} // namespace KCE
