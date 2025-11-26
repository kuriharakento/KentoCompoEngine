#pragma once
#include "externals/nlohmann/json.hpp"
#include "jsonEditor/JsonEditableBase.h"
#include "math/Vector3.h"

using json = nlohmann::json;

/**
 * @brief スプライン制御点データクラス
 * 
 * スプライン補間に使用する制御点を管理するクラス。
 * JSONファイルからの読み込み・保存、ImGuiによる編集に対応。
 */
class SplineData : public JsonEditableBase
{
public:
	/**
	 * @brief コンストラクタ
	 * 
	 * JSONシリアライズ用に制御点配列を登録する。
	 */
	SplineData();

	/**
	 * @brief 初期化処理
	 * @param name JSONファイルのパス
	 */
	void Initialize(const std::string& name);

	/**
	 * @brief 制御点を追加
	 * @param point 追加する制御点の座標
	 */
	void AddControlPoint(const Vector3& point) { controlPoints.push_back(point); }

	/**
	 * @brief 制御点配列を取得
	 * @return 制御点のconst参照
	 */
	const std::vector<Vector3>& GetControlPoints() const { return controlPoints; }

	/**
	 * @brief ImGuiによる編集UIを描画
	 */
	void DrawImGui() override;

private:
	// 制御点の座標配列
	std::vector<Vector3> controlPoints;
};

