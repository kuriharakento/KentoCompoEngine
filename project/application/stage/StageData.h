#pragma once
// json
#include <nlohmann/json.hpp>
// math
#include <base/GraphicsTypes.h>
// editor
#include "engine/jsonEditor/JsonEditableBase.h"
/**
 * \brief Jsonから読み込むゲームオブジェクトの情報を格納する構造体。
 */
struct GameObjectInfo
{
	std::string type;		// ゲームオブジェクトのタイプ
	std::string name;		// モデル名
	bool disabled;			// 無効化フラグ
	Transform transform;	// トランスフォーム情報
};
// Jsonシリアライズ用のマクロ
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GameObjectInfo, type, name, disabled, transform)

class StageData : public JsonEditableBase
{
public:
	StageData();
	void DrawImGui() override;
	std::vector<GameObjectInfo> gameObjects; // ゲームオブジェクトのリスト
};

