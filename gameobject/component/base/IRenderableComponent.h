#pragma once

namespace KCE
{
class IRenderable3d;

namespace GameObjectComponent
{
/**
 * @brief 描く物を持つコンポーネントの共通の顔
 *
 * - GameObjectRenderer は、GameObject 本体の描画物とは別に、これを持つコンポーネントも集めて描く
 * - 持ち主の GameObject が消えるときに一緒に消える前提。描く物の所有はコンポーネント側
 */
class IRenderableComponent
{
public:
	virtual ~IRenderableComponent() = default;

	/**
	 * @brief 描く物を返す
	 * @return 描く物。まだ用意できていなければ nullptr。所有権は移らない
	 */
	virtual IRenderable3d* GetRenderable3d() const = 0;

	/**
	 * @brief 持ち主のワールド行列を描く物へ流す
	 * @details フレームに1回、GameObject::EnsureRenderTransform から呼ばれる
	 */
	virtual void UpdateRenderTransform() = 0;

	/** @brief 影を落とすか */
	virtual bool GetCastShadow() const = 0;
};
} // namespace GameObjectComponent
} // namespace KCE
