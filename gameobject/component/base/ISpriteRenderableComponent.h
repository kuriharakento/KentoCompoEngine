#pragma once

namespace KCE
{
namespace GameObjectComponent
{
/**
 * @brief 画面（1920x1080）へ2Dで描く物を持つコンポーネントの共通の顔
 *
 * - GameObjectManager::DrawSprites2D が、木をたどって集め、order の小さい順に描く（同じなら木をたどった順）
 * - 描く前に、持ち主の GameObject::EnsureRenderTransform でワールド行列が確定している
 *   （Transform の XY が画面の px、Z 回転が画面内の回転）
 * - Behaviour::Draw2D とは別。Draw2D は order を持たず、今は呼ばれていない
 */
class ISpriteRenderableComponent
{
public:
	virtual ~ISpriteRenderableComponent() = default;

	/** @brief 描く順。小さいほど奥 */
	virtual int GetSpriteOrder() const = 0;

	/**
	 * @brief Transform の位置が画面の px で決まるなら真（3D のワールドに置くゲージは偽）
	 * @details インスペクタの位置のドラッグ幅を、px 向けに粗くするために使う
	 */
	virtual bool IsScreenSpace() const { return true; }

	/**
	 * @brief 描く
	 * @details 呼ばれる時点で、スプライト用のパイプラインが設定済み。
	 *          別のパイプラインに切り替えたときは、戻してから返す
	 */
	virtual void DrawSprite2D() = 0;
};
} // namespace GameObjectComponent
} // namespace KCE
