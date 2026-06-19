#pragma once
#include <nlohmann/json.hpp>
#include "base/GraphicsTypes.h"

/**
 * @brief TransformをJSONにシリアライズ
 * @param j 出力先のJSONオブジェクト
 * @param t シリアライズするTransform
 */
void to_json(nlohmann::json& j, Transform const& t);

/**
 * @brief JSONからTransformにデシリアライズ
 * 
 * translate/position/translation、rotate/rotation、scale/scalingの
 * いずれのキー名にも対応します。
 * @param j 入力元のJSONオブジェクト
 * @param t デシリアライズ先のTransform
 */
void from_json(nlohmann::json const& j, Transform& t);

/**
 * @brief Vector3をJSONにシリアライズ
 * @param j 出力先のJSONオブジェクト
 * @param v シリアライズするVector3
 */
void to_json(nlohmann::json& j, Vector3 const& v);

/**
 * @brief JSONからVector3にデシリアライズ
 * 
 * {"x":..., "y":..., "z":...}形式と[x, y, z]配列形式の両方に対応します。
 * @param j 入力元のJSONオブジェクト
 * @param v デシリアライズ先のVector3
 */
void from_json(nlohmann::json const& j, Vector3& v);

/**
 * @brief Vector4をJSONにシリアライズ
 */
void to_json(nlohmann::json& j, Vector4 const& v);

/**
 * @brief JSONからVector4にデシリアライズ
 */
void from_json(nlohmann::json const& j, Vector4& v);

