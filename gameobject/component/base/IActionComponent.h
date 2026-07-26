#pragma once
#include "IGameObjectComponent.h"

namespace KCE
{
/**
 * @brief アクションコンポーネントの基底インターフェース
 * 
 * GameObjectに動的な振る舞いを追加するコンポーネントの基底クラスです。
 * 移動、射撃、アニメーション、AIなどの機能を実装するために使用されます。
 * 
 * @note 描画機能を持つコンポーネントはDraw()をオーバーライドします
 */
namespace GameObjectComponent
{
	class IActionComponent : public virtual IGameObjectComponent
	{
	public:
		virtual ~IActionComponent() = default;
		
		/**
		 * @brief 3D描画処理
		 * 
		 * コンポーネントの3D描画処理を実装します。
		 * 必要に応じてオーバーライドしてください。
		 * 
		 * @param camera 描画に使用するカメラマネージャー
		 */
		virtual void Draw3D(CameraManager* camera) {}

		/**
		 * @brief 2D描画処理
		 * 
		 * コンポーネントの2D描画処理を実装します。
		 * 必要に応じてオーバーライドしてください。
		 */
		virtual void Draw2D() {}
	};
}
} // namespace KCE
