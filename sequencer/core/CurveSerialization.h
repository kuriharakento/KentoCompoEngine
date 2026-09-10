#pragma once
#include <nlohmann/json.hpp>

#include "jsonEditor/JsonSerialization.h"
#include "sequencer/core/Curve.h"

namespace KCE
{
/**
 * @brief Quaternion を JSON に書き出す
 * @param json 出力先
 * @param q 対象
 */
inline void QuaternionToJson(nlohmann::json& json, const Quaternion& q)
{
	json = nlohmann::json::array({ q.x, q.y, q.z, q.w });
}

/**
 * @brief JSON から Quaternion を読み込む
 * @details [x, y, z, w] 配列形式と {"x":..,"y":..,"z":..,"w":..} 形式の両方に対応する。
 * @param json 入力
 * @param q 出力先
 * @return 読み込めたら真
 */
inline bool QuaternionFromJson(const nlohmann::json& json, Quaternion& q)
{
	if (json.is_array() && json.size() == 4)
	{
		q.x = json[0].get<float>();
		q.y = json[1].get<float>();
		q.z = json[2].get<float>();
		q.w = json[3].get<float>();
		return true;
	}
	if (json.is_object() && json.contains("x") && json.contains("y") && json.contains("z") && json.contains("w"))
	{
		q.x = json["x"].get<float>();
		q.y = json["y"].get<float>();
		q.z = json["z"].get<float>();
		q.w = json["w"].get<float>();
		return true;
	}
	return false;
}

/**
 * @brief カーブの値の型ごとのJSON変換
 * @details Curve<T> をシリアライズするために、値の型ごとに特殊化する。
 */
template<class T>
struct CurveValueSerializer;

/** @brief float 用 */
template<>
struct CurveValueSerializer<float>
{
	static nlohmann::json ToJson(const float& value) { return value; }
	static bool FromJson(const nlohmann::json& json, float& value)
	{
		if (!json.is_number()) { return false; }
		value = json.get<float>();
		return true;
	}
};

/** @brief Vector3 用 */
template<>
struct CurveValueSerializer<Vector3>
{
	static nlohmann::json ToJson(const Vector3& value)
	{
		return nlohmann::json::array({ value.x, value.y, value.z });
	}
	static bool FromJson(const nlohmann::json& json, Vector3& value)
	{
		if (json.is_array() && json.size() == 3)
		{
			value.x = json[0].get<float>();
			value.y = json[1].get<float>();
			value.z = json[2].get<float>();
			return true;
		}
		if (json.is_object() && json.contains("x") && json.contains("y") && json.contains("z"))
		{
			from_json(json, value);
			return true;
		}
		return false;
	}
};

/** @brief Vector4 用 */
template<>
struct CurveValueSerializer<Vector4>
{
	static nlohmann::json ToJson(const Vector4& value)
	{
		return nlohmann::json::array({ value.x, value.y, value.z, value.w });
	}
	static bool FromJson(const nlohmann::json& json, Vector4& value)
	{
		if (json.is_array() && json.size() == 4)
		{
			value.x = json[0].get<float>();
			value.y = json[1].get<float>();
			value.z = json[2].get<float>();
			value.w = json[3].get<float>();
			return true;
		}
		if (json.is_object() && json.contains("x") && json.contains("y") && json.contains("z") && json.contains("w"))
		{
			from_json(json, value);
			return true;
		}
		return false;
	}
};

/** @brief Quaternion 用 */
template<>
struct CurveValueSerializer<Quaternion>
{
	static nlohmann::json ToJson(const Quaternion& value)
	{
		nlohmann::json json;
		QuaternionToJson(json, value);
		return json;
	}
	static bool FromJson(const nlohmann::json& json, Quaternion& value)
	{
		return QuaternionFromJson(json, value);
	}
};

/**
 * @brief 補間モードを文字列に変換する
 * @param mode 補間モード
 * @return 文字列
 */
inline const char* InterpolationModeToString(InterpolationMode mode)
{
	switch (mode)
	{
	case InterpolationMode::Constant: return "constant";
	case InterpolationMode::Linear:   return "linear";
	case InterpolationMode::Bezier:   return "bezier";
	default:                          return "bezier";
	}
}

/**
 * @brief 文字列から補間モードを復元する
 * @param str 文字列
 * @return 補間モード。未知の文字列なら Bezier
 */
inline InterpolationMode InterpolationModeFromString(const std::string& str)
{
	if (str == "constant") { return InterpolationMode::Constant; }
	if (str == "linear") { return InterpolationMode::Linear; }
	return InterpolationMode::Bezier;
}

/**
 * @brief カーブをJSONに書き出す
 * @param curve 対象のカーブ
 * @return キーの配列
 */
template<class T>
nlohmann::json SerializeCurve(const Curve<T>& curve)
{
	nlohmann::json keysJson = nlohmann::json::array();
	for (const auto& key : curve.GetKeys())
	{
		nlohmann::json keyJson;
		keyJson["time"] = key.time;
		keyJson["value"] = CurveValueSerializer<T>::ToJson(key.value);
		keyJson["interp"] = InterpolationModeToString(key.interp);
		// ベジェ以外でも制御点は保持する。補間モードを切り替えても形状が失われないようにするため。
		keyJson["bezier"] = nlohmann::json::array({ key.bezier.x1, key.bezier.y1, key.bezier.x2, key.bezier.y2 });
		keysJson.push_back(keyJson);
	}
	return keysJson;
}

/**
 * @brief JSONからカーブを復元する
 * @details 読めなかったキーは読み飛ばす。1つでも読めれば真を返す。
 * @param json キーの配列
 * @param curve 出力先。呼び出し前に内容は破棄される
 * @return 配列として解釈できたら真
 */
template<class T>
bool DeserializeCurve(const nlohmann::json& json, Curve<T>& curve)
{
	curve.Clear();
	if (!json.is_array())
	{
		return false;
	}

	for (const auto& keyJson : json)
	{
		if (!keyJson.is_object() || !keyJson.contains("time") || !keyJson.contains("value"))
		{
			continue;
		}

		Keyframe<T> key;
		key.time = keyJson["time"].get<float>();
		if (!CurveValueSerializer<T>::FromJson(keyJson["value"], key.value))
		{
			continue;
		}

		if (keyJson.contains("interp") && keyJson["interp"].is_string())
		{
			key.interp = InterpolationModeFromString(keyJson["interp"].get<std::string>());
		}

		if (keyJson.contains("bezier") && keyJson["bezier"].is_array() && keyJson["bezier"].size() == 4)
		{
			key.bezier.x1 = keyJson["bezier"][0].get<float>();
			key.bezier.y1 = keyJson["bezier"][1].get<float>();
			key.bezier.x2 = keyJson["bezier"][2].get<float>();
			key.bezier.y2 = keyJson["bezier"][3].get<float>();
		}

		curve.AddKey(key);
	}

	return true;
}
} // namespace KCE
