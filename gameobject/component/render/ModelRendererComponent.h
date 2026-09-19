#pragma once
#include <memory>
#include <string>

#include "engine/gameobject/component/base/Component.h"
#include "engine/gameobject/component/base/IRenderableComponent.h"
#include "graphics/3d/Object3d.h"
#include "jsonEditor/JsonEditableBase.h"

namespace KCE
{
namespace GameObjectComponent
{
/**
 * @brief 静的モデルを1つ描くコンポーネント
 *
 * - GameObject 本体の SetModel と同じことを、コンポーネントとして持てるようにしたもの
 * - 描く物の一覧は GameObjectRenderer が集める。ワールド行列は持ち主の物をそのまま使う
 * - 1つの GameObject に複数付けてよい（本体の描画物とも同居できる）
 */
class ModelRendererComponent : public Component, public IRenderableComponent, public JsonEditableBase
{
public:
	ModelRendererComponent();

	/** @brief 持ち主が決まったところで Object3d を用意する */
	void Awake() override;

	IRenderable3d* GetRenderable3d() const override { return object3d_.get(); }
	void UpdateRenderTransform() override;
	bool GetCastShadow() const override { return castShadow_; }

	/**
	 * @brief 描くモデルを設定する
	 * @param modelName ModelManager に読ませるモデル名
	 */
	void SetModel(const std::string& modelName);
	/** @return 設定されているモデル名 */
	const std::string& GetModelName() const { return modelName_; }

	/** @return 中の Object3d。所有権は移らない。用意できていなければ nullptr */
	Object3d* GetObject3d() const { return object3d_.get(); }

	/** @brief 影を落とすかを設定する */
	void SetCastShadow(bool cast);
	/** @brief 色を設定する */
	void SetColor(const Vector4& color);

private:
	/** @brief modelName_ や castShadow_ の変更を Object3d へ反映する */
	void ApplySettings();

	// このコンポーネントが所有する描画物
	std::unique_ptr<Object3d> object3d_;
	// 描くモデルの名前。プレハブに保存する
	std::string modelName_;
	// 影を落とすか。プレハブに保存する
	bool castShadow_ = true;
	// Object3d へ反映済みのモデル名。プレハブ読み込み後の反映を拾うために持つ
	std::string appliedModelName_;
	bool appliedCastShadow_ = true;
};
} // namespace GameObjectComponent
} // namespace KCE
