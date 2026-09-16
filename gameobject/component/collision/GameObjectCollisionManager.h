#pragma once
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

#include "AABBCollider.h"
#include "CollisionAlgorithm.h"
#include "OBBCollider.h"
#include "engine/gameobject/component/base/Collider.h"
#include "engine/math/Ray.h"

namespace KCE
{
namespace GameObjectComponent
{
	class Collider;
	class AABBCollider;
	class OBBCollider;
	class SphereCollider;
}

/**
 * @brief 衝突判定を統括管理するマネージャークラス（シングルトン）
 *
 * 登録された全てのコライダー間の衝突判定を一括で行います。
 * 2D/3Dの切り替えや、各種コライダー組み合わせの判定を自動で振り分けます。
 *
 * @note シングルトンパターンで実装されています
 */
class GameObjectCollisionManager
{
public:
	/** @brief レイが最初に当たった相手。 */
	struct RaycastHit
	{
		GameObject* object = nullptr;
		GameObjectComponent::Collider* collider = nullptr;
		float distance = 0.0f;
		Vector3 position = {};
	};
	/**
	 * @brief シングルトンインスタンスを取得
	 * @return GameObjectCollisionManagerのインスタンス
	 */
	static GameObjectCollisionManager* GetInstance();

	/**
	 * @brief インスタンスがあるか
	 * @return あれば真。外すだけの処理（Unregister）は、無ければ作らずに何もしない
	 */
	static bool HasInstance();

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
	 * Colliderのコンストラクタから自動的に呼び出されます。
	 *
	 * @param collider 登録するコライダー
	 */
	void Register(GameObjectComponent::Collider* collider);

	/**
	 * @brief コライダーを登録解除
	 *
	 * Colliderのデストラクタから自動的に呼び出されます。
	 *
	 * @param collider 登録解除するコライダー
	 */
	void Unregister(GameObjectComponent::Collider* collider);

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
	 * シーンの Update で、この関数、GameObjectManager::Update、CheckCollisions の順に呼ぶ。
	 */
	void UpdatePreviousPositions();
	/** @brief レイヤーマスク内でレイに最も近い相手を探す。 */
	bool Raycast(const Ray& ray, uint32_t mask, RaycastHit& outHit) const;

	/**
	 * @brief 登録されている全てのコライダーを取得（読み取り専用）
	 * @return コライダーのリスト
	 */
	const std::vector<GameObjectComponent::Collider*>& GetColliders() const { return colliders_; }

	/**
	 * @brief 空間分割用のグリッドセルサイズを設定
	 * @param size セルの1辺の長さ（m単位。ゲーム内の最大コライダーサイズ程度を推奨）
	 */
	void SetCellSize(float size) { cellSize_ = size; }

	/**
	 * @brief 空間分割用のグリッドセルサイズを取得
	 * @return セルの1辺の長さ
	 */
	float GetCellSize() const { return cellSize_; }
	/** @brief 現在触れているコライダー組数を返す。 */
	size_t GetActiveCollisionCount() const { return currentCollisions_.size(); }

private:
	// シングルトンインスタンス
	static std::unique_ptr<GameObjectCollisionManager> instance_;
	friend std::unique_ptr<GameObjectCollisionManager> std::make_unique<GameObjectCollisionManager>();

	GameObjectCollisionManager() = default;
	GameObjectCollisionManager(const GameObjectCollisionManager&) = delete;
	GameObjectCollisionManager& operator=(const GameObjectCollisionManager&) = delete;

public:
	~GameObjectCollisionManager();

#ifdef USE_IMGUI
	void DrawImGui();
#endif

private:
	// 衝突判定関数テーブル用型定義
	using CollisionCheckFunc = bool(*)(const GameObjectComponent::Collider*, const GameObjectComponent::Collider*, Vector3& outMtv);

	// 衝突判定関数テーブルマトリクス
	// 4x4 (AABB=0, Sphere=1, OBB=2, Ray=3)
	CollisionCheckFunc collisionMatrix_[4][4] = {};

	// CCD（サブステップ）用衝突判定関数テーブルマトリクス
	CollisionCheckFunc ccdMatrix_[4][4] = {};

	// コライダータイプから文字列を取得
	std::string GetColliderTypeString(ColliderType type) const;

	// 衝突をログに出力
	void LogCollision(const std::string& phase, const GameObjectComponent::Collider* a, const GameObjectComponent::Collider* b);

	/**
	 * @brief 衝突ペアを識別するための構造体
	 */
	struct CollisionPair
	{
		const GameObjectComponent::Collider* a;
		const GameObjectComponent::Collider* b;

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
	std::vector<GameObjectComponent::Collider*> colliders_;

	// 現在接触しているペア（状態追跡用）
	std::unordered_set<CollisionPair, CollisionPairHash> currentCollisions_;
	std::unordered_set<CollisionPair, CollisionPairHash> nextCollisions_;
	struct CollisionDetails
	{
		Vector3 normal = {};
		float depth = 0.0f;
		Vector3 point = {};
	};
	std::unordered_map<CollisionPair, CollisionDetails, CollisionPairHash> collisionDetails_;
	struct ObjectPair
	{
		GameObject* a;
		GameObject* b;
		bool operator==(const ObjectPair& other) const { return a == other.a && b == other.b; }
	};
	struct ObjectPairHash
	{
		std::size_t operator()(const ObjectPair& pair) const { return std::hash<const void*>()(pair.a) ^ (std::hash<const void*>()(pair.b) << 1); }
	};
	std::unordered_set<ObjectPair, ObjectPairHash> currentObjectCollisions_;
	std::unordered_set<ObjectPair, ObjectPairHash> nextObjectCollisions_;

	// 通知は写しで回す。通知の中で登録や解除が起きても、回している一覧が壊れないようにするため。毎フレーム確保しないよう使い回す
	std::vector<CollisionPair> dispatchPairs_;
	std::vector<ObjectPair> dispatchObjectPairs_;
	// このフレームで Enter を送った組。通知の途中で外されたとき、Exit も送るかの判断に使う
	std::vector<CollisionPair> enteredPairs_;
	std::vector<ObjectPair> enteredObjectPairs_;
	// 通知の途中で外されたコライダーと、破棄中の持ち主。解放済みかもしれないので、アドレスを比べるだけに使う
	std::vector<const GameObjectComponent::Collider*> removedColliders_;
	std::vector<const GameObject*> destroyedOwners_;
	// 通知の入れ子の深さ。0 に戻ったら上の2つを空にする
	int dispatchDepth_ = 0;

	/** @brief 通知の入れ子を1段深くする。 */
	void BeginDispatch() { ++dispatchDepth_; }
	/** @brief 通知の入れ子を1段戻す。いちばん外まで戻ったら、外した記録を消す。 */
	void EndDispatch();
	/**
	 * @brief コライダー組の Exit を両側に送る。
	 * @param pair 送る組。呼ぶ前に一覧から抜いておく
	 * @param removing いま外している最中のコライダー。記録にあっても生きているので通知してよい。無ければ nullptr
	 */
	void NotifyColliderExit(const CollisionPair& pair, const GameObjectComponent::Collider* removing);
	/**
	 * @brief オブジェクト組の Exit を両側に送る。破棄中の側には送らない。
	 * @param objects 送る組。呼ぶ前に一覧から抜いておく
	 */
	void NotifyObjectExit(const ObjectPair& objects);
	/**
	 * @brief このフレームの通知の途中で外されたか。
	 * @param collider 調べるコライダー。解放済みでもよい（アドレスを比べるだけ）
	 * @return 外されていれば真
	 */
	bool WasRemoved(const GameObjectComponent::Collider* collider) const;

	// 空間分割用セルキー構造体
	struct CellKey
	{
		int x, y, z;

		bool operator==(const CellKey& other) const
		{
			return x == other.x && y == other.y && z == other.z;
		}
	};

	// セルキーハッシュ構造体
	struct CellKeyHash
	{
		std::size_t operator()(const CellKey& key) const
		{
			return (std::hash<int>()(key.x) ^ (std::hash<int>()(key.y) << 1)) >> 1 ^ (std::hash<int>()(key.z) << 1);
		}
	};

	// 空間グリッド分割バケット
	std::unordered_map<CellKey, std::vector<GameObjectComponent::Collider*>, CellKeyHash> gridBuckets_;
	// Raycast ごとに確保しないよう候補領域を使い回す。
	mutable std::vector<GameObjectComponent::Collider*> raycastCandidates_;

	// 空間分割のセルサイズ
	float cellSize_ = 15.0f; // デフォルト 15m 四方
};
} // namespace KCE
