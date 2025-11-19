#pragma once

class CameraManager;
class GameObject;

/**
 * @brief 全てのコンポーネントの基底インターフェース
 * 
 * Entity-Component-System(ECS)パターンにおけるコンポーネントの基底クラスです。
 * GameObjectに機能を追加するための統一インターフェースを提供します。
 * 
 * 継承階層:
 * IGameObjectComponent
 * ├── IActionComponent (移動、射撃、アニメーションなど)
 * └── ICollisionComponent (衝突判定)
 */
class IGameObjectComponent
{
public:
	virtual ~IGameObjectComponent() = default;
	
	/**
	 * @brief 毎フレームの更新処理
	 * 
	 * コンポーネントの機能を更新します。
	 * GameObjectのUpdate()から呼び出されます。
	 * 
	 * @param owner このコンポーネントを所有するGameObject
	 */
	virtual void Update(GameObject* owner) = 0;
};