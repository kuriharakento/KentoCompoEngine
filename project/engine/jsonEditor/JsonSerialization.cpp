#include "JsonSerialization.h"

void to_json(nlohmann::json& j, Transform const& t)
{
    j = {
      {"translate", t.translate},
      {"rotate", t.rotate},
      {"scale", t.scale}
    };
}

void from_json(nlohmann::json const& j, Transform& t)
{
	// 名前を変換
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

    //
	if (j.contains("rotate"))
	{
		j.at("rotate").get_to(t.rotate);
	}
	else if (j.contains("rotation"))
	{
		j.at("rotation").get_to(t.rotate);
	}

    // 
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
    j = { {"x", v.x}, {"y", v.y}, {"z", v.z} };
}

void from_json(nlohmann::json const& j, Vector3& v)
{
    if (j.is_object())
    {
        if (j.contains("x") && j.contains("y") && j.contains("z"))
        {
            v.x = j.at("x").get<float>();
            v.y = j.at("y").get<float>();
            v.z = j.at("z").get<float>();
        }
    }
    else if (j.is_array() && j.size() == 3)
    {
        v.x = j[0].get<float>();
        v.y = j[1].get<float>();
        v.z = j[2].get<float>();
    }
}