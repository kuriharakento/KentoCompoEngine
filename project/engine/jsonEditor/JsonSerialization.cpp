#include "JsonSerialization.h"

void to_json(nlohmann::json& j, Transform const& t)
{
    // 標準的なキー名でシリアライズ
    j = {
      {"translate", t.translate},
      {"rotate", t.rotate},
      {"scale", t.scale}
    };
}

void from_json(nlohmann::json const& j, Transform& t)
{
	// 位置の読み込み（複数のキー名に対応）
    if (j.contains("translate"))
    {
        j.at("translate").get_to(t.translate);
    }
    else if (j.contains("position"))
    {
        j.at("position").get_to(t.translate);
    }
	else if (j.contains("translation"))
	{
		j.at("translation").get_to(t.translate);
	}

    // 回転の読み込み（複数のキー名に対応）
	if (j.contains("rotate"))
	{
		j.at("rotate").get_to(t.rotate);
	}
	else if (j.contains("rotation"))
	{
		j.at("rotation").get_to(t.rotate);
	}

    // スケールの読み込み（複数のキー名に対応）
	if (j.contains("scale"))
	{
		j.at("scale").get_to(t.scale);
	}
	else if (j.contains("scaling"))
	{
		j.at("scaling").get_to(t.scale);
	}
}

void to_json(nlohmann::json& j, Vector3 const& v)
{
    // オブジェクト形式でシリアライズ
    j = { {"x", v.x}, {"y", v.y}, {"z", v.z} };
}

void from_json(nlohmann::json const& j, Vector3& v)
{
    // オブジェクト形式の読み込み
    if (j.is_object())
    {
        if (j.contains("x") && j.contains("y") && j.contains("z"))
        {
            v.x = j.at("x").get<float>();
            v.y = j.at("y").get<float>();
            v.z = j.at("z").get<float>();
        }
    }
    // 配列形式の読み込み
    else if (j.is_array() && j.size() == 3)
    {
        v.x = j[0].get<float>();
        v.y = j[1].get<float>();
        v.z = j[2].get<float>();
    }
}

void to_json(nlohmann::json& j, Vector4 const& v)
{
    j = { {"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w} };
}

void from_json(nlohmann::json const& j, Vector4& v)
{
    if (j.is_object())
    {
        if (j.contains("x")) v.x = j.at("x").get<float>();
        if (j.contains("y")) v.y = j.at("y").get<float>();
        if (j.contains("z")) v.z = j.at("z").get<float>();
        if (j.contains("w")) v.w = j.at("w").get<float>();
        // RGBA形式にも対応
        if (j.contains("r")) v.x = j.at("r").get<float>();
        if (j.contains("g")) v.y = j.at("g").get<float>();
        if (j.contains("b")) v.z = j.at("b").get<float>();
        if (j.contains("a")) v.w = j.at("a").get<float>();
    }
    else if (j.is_array() && j.size() == 4)
    {
        v.x = j[0].get<float>();
        v.y = j[1].get<float>();
        v.z = j[2].get<float>();
        v.w = j[3].get<float>();
    }
}