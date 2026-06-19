#pragma once
#include <unordered_set>
#include <memory>
#include <vector>
#include <string>

#include "AABBColliderComponent.h"
#include "CollisionAlgorithm.h"
#include "OBBColliderComponent.h"
#include "engine/gameobject/component/base/ICollisionComponent.h"

namespace GameObjectComponent
{
	class ICollisionComponent;
	class AABBColliderComponent;
	class OBBColliderComponent;
	class SphereColliderComponent;
}

/**
 * @brief 衝突判定の次元モードを表す列挙型
 */
enum class CollisionDimension
{
	Mode2D,	// 2D判定モード（横スクロールゲームなど）
	Mode3D	// 3D判定モード
};

/**
 * @brief 衝突判定を統括管理するマネージャークラス（シングルトン）
 * 
 * 登録された全てのコライダー間の衝突判定を一括で行います。
 * 2D/3Dの切り替えや、各種コライダー組み合わせの判定を自動で振り分けます。
 * 
 * @note シングルトンパターンで実装されています
 */
class CollisionManager
{
public:
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return CollisionManagerのインスタンス
	 */
	static CollisionManager* GetInstance();
	
	/**
	 * @brief マネージャーを初期化
	 * 
	 * 登録されているコライダーをクリアします。
	 */
	void Initialize();
	
	/**
	 * @brief マネージャーを終了
	 * 
	 * 全てのコライダーと衝突情報をクリアします。
	 */
	void Finalize();

	/**
	 * @brief コライダーを登録
	 * 
	 * ICollisionComponentのコンストラクタから自動的に呼び出されます。
	 * 
	 * @param collider 登録するコライダー
	 */
	void Register(GameObjectComponent::ICollisionComponent* collider);
	
	/**
	 * @brief コライダーを登録解除
	 * 
	 * ICollisionComponentのデストラクタから自動的に呼び出されます。
	 * 
	 * @param collider 登録解除するコライダー
	 */
	void Unregister(GameObjectComponent::ICollisionComponent* collider);
	
	/**
	 * @brief 全コライダー間の衝突判定を実行
	 * 
	 * 登録されている全てのコライダー同士の組み合わせで判定を行います。
	 * 衝突状態の変化に応じてコールバックを呼び出します。
	 */
	void CheckCollisions();
	
	/**
	 * @brief 全コライダーの前フレーム位置を更新
	 * 
	 * サブステップ判定のために現在位置を保存します。
	 * フレームの最初に呼び出してください。
	 */
	void UpdatePreviousPositions();

	/**
	 * @brief 衝突判定の次元を設定
	 * 
	 * 2D横スクロールゲームなどでは2Dモードを使用します。
	 * 
	 * @param dimension 判定次元（Mode2D or Mode3D）
	 */
	void SetCollisionDimension(CollisionDimension dimension) { dimension_ = dimension; }

	/**
	 * @brief 2Dモード時の衝突判定面を設定
	 * 
	 * 2Dモードでどの平面で判定を行うかを指定します。
	 * 
	 * @param plane 判定平面（XY, XZ, YZ）
	 */
	void SetCollisionPlane(CollisionPlane plane) { collisionPlane_ = plane; }

	/**
	 * @brief 登録されている全てのコライダーを取得（読み取り専用）
	 * @return コライダーのリスト
	 */
	const std::vector<GameObjectComponent::ICollisionComponent*>& GetColliders() const { return colliders_; }

private:
	// シングルトンインスタンス
	static std::unique_ptr<CollisionManager> instance_;
	
	CollisionManager() = default;
	CollisionManager(const CollisionManager&) = delete;
	CollisionManager& operator=(const CollisionManager&) = delete;

public:
	~CollisionManager();

#ifdef USE_IMGUI
	void DrawImGui();
#endif

private:
	// 衝突判定関数（3D）
	bool CheckCollision(const GameObjectComponent::AABBColliderComponent* a, const GameObjectComponent::AABBColliderComponent* b);
	bool CheckCollision(const GameObjectComponent::OBBColliderComponent* a, const GameObjectComponent::OBBColliderComponent* b);
	bool CheckCollision(const GameObjectComponent::AABBColliderComponent* a, const GameObjectComponent::OBBColliderComponent* b);

	// 衝突判定関数（サブステップ）
	bool CheckSubstepCollision(const GameObjectComponent::AABBColliderComponent* a, const GameObjectComponent::AABBColliderComponent* b);
	bool CheckSubstepCollision(const GameObjectComponent::OBBColliderComponent* a, const GameObjectComponent::OBBColliderComponent* b);
	bool CheckSubstepCollision(const GameObjectComponent::AABBColliderComponent* a, const GameObjectComponent::OBBColliderComponent* b);

	// コライダータイプから文字列を取得
	std::string GetColliderTypeString(ColliderType type) const;
	
	// 衝突をログに出力
	void LogCollision(const std::string& phase, const GameObjectComponent::ICollisionComponent* a, const GameObjectComponent::ICollisionComponent* b);

	/**
	 * @brief 衝突ペアを識別するための構造体
	 */
	struct CollisionPair
	{
		const GameObjectComponent::ICollisionComponent* a;
		const GameObjectComponent::ICollisionComponent* b;

		bool operator==(const CollisionPair& other) const
		{
			return (a == other.a && b == other.b) || (a == other.b && b == other.a);
		}
	};

	/**
	 * @brief 衝突ペアのハッシュ関数
	 */
	struct CollisionPairHash
	{
		std::size_t operator()(const CollisionPair& pair) const
		{
			return std::hash<const void*>()(pair.a) ^ std::hash<const void*>()(pair.b);
		}
	};

	// 登録されているコライダーのリスト
	std::vector<GameObjectComponent::ICollisionComponent*> colliders_;

	// 現在接触しているペア（状態追跡用）
	std::unordered_set<CollisionPair, CollisionPairHash> currentCollisions_;

	// 衝突判定の次元（2D or 3D）
	CollisionDimension dimension_ = CollisionDimension::Mode3D;

	// 2Dモード時の衝突判定面
	CollisionPlane collisionPlane_ = CollisionPlane::XY;
};
