#pragma once
#include "IGameObjectComponent.h"

/**
 * @brief アクションコンポーネントの基底インターフェース
 * 
 * GameObjectに動的な振る舞いを追加するコンポーネントの基底クラスです。
 * 移動、射撃、アニメーション、AIなどの機能を実装するために使用されます。
 * 
 * @note 描画機能を持つコンポーネントはDraw()をオーバーライドします
 */
class IActionComponent : public virtual IGameObjectComponent
{
public:
	virtual ~IActionComponent() = default;
	
	/**
	 * @brief 描画処理
	 * 
	 * コンポーネントの描画処理を実装します。
	 * 必要に応じてオーバーライドしてください。
	 * 
	 * @param camera 描画に使用するカメラマネージャー
	 */
	virtual void Draw(CameraManager* camera) {}
};
