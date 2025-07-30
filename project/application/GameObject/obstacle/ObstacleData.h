#pragma once
#include "base/GraphicsTypes.h"
#include "externals/nlohmann/json.hpp"
#include "jsonEditor/JsonEditableBase.h"
#include "math/Vector3.h"

using json = nlohmann::json;

struct ObstacleInfo
{
	std::string type; // 障害物の種類
	std::string name; // 障害物の名前
	bool disabled; // 障害物が無効かどうか
	Transform transform; // 障害物のTransform情報
};
// JSONシリアライズ用のマクロ
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObstacleInfo, type, name, disabled, transform)

class ObstacleData : public JsonEditableBase
{
public:
    ObstacleData();
    void Initialize(const std::string& name);
    void AddObstacle(const Vector3& position, const Vector3& rotation, const Vector3& scale);
    void DrawImGui() override;
	std::vector<ObstacleInfo>& GetObstacles() { return obstacles; } // 障害物のTransform情報を取得
	uint32_t GetObstacleCount() const { return static_cast<uint32_t>(obstacles.size()); } // 障害物の数を取得

private:
	std::vector<ObstacleInfo> obstacles; // 障害物のTransform情報
};