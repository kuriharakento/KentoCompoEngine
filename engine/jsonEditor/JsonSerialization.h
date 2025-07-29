#pragma once
#include <nlohmann/json.hpp>
#include "base/GraphicsTypes.h"

void to_json(nlohmann::json& j, Transform const& t);
void from_json(nlohmann::json const& j, Transform& t);
void to_json(nlohmann::json& j, Vector3 const& v);
void from_json(nlohmann::json const& j, Vector3& v);