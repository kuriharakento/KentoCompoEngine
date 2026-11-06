#pragma once

#include <nlohmann/json.hpp>

#include "math/Vector3.h"
#include "math/Vector4.h"

namespace KCE
{
/**
 * @brief SettingsStore に登録する項目の、JSON の読み書きの小道具
 * @details 読み込みは、キーが無い・型が違うときは何もしない（既定値のまま）。例外は出さない
 */
namespace SettingsJson
{
inline nlohmann::json ToJson(const Vector3& v) { return nlohmann::json::array({ v.x, v.y, v.z }); }
inline nlohmann::json ToJson(const Vector4& v) { return nlohmann::json::array({ v.x, v.y, v.z, v.w }); }

/** @brief json[key] が count 個の数の配列なら out へ読む */
inline bool ReadFloats(const nlohmann::json& json, const char* key, size_t count, float* out)
{
	const auto found = json.find(key);
	if (found == json.end() || !found->is_array() || found->size() != count)
	{
		return false;
	}
	for (size_t i = 0; i < count; ++i)
	{
		if (!(*found)[i].is_number()) { return false; }
	}
	for (size_t i = 0; i < count; ++i)
	{
		out[i] = (*found)[i].get<float>();
	}
	return true;
}

inline void Read(const nlohmann::json& json, const char* key, Vector3& out) { ReadFloats(json, key, 3, &out.x); }
inline void Read(const nlohmann::json& json, const char* key, Vector4& out) { ReadFloats(json, key, 4, &out.x); }

inline void Read(const nlohmann::json& json, const char* key, bool& out)
{
	const auto found = json.find(key);
	if (found != json.end() && found->is_boolean()) { out = found->get<bool>(); }
}

inline void Read(const nlohmann::json& json, const char* key, float& out)
{
	const auto found = json.find(key);
	if (found != json.end() && found->is_number()) { out = found->get<float>(); }
}

inline void Read(const nlohmann::json& json, const char* key, int& out)
{
	const auto found = json.find(key);
	if (found != json.end() && found->is_number_integer()) { out = found->get<int>(); }
}
} // namespace SettingsJson
} // namespace KCE
