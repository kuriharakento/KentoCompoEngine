#pragma once
#include <functional>

#include "IGameObjectComponent.h"
#include "math/Vector3.h"
#include "math/AABB.h"
#include "engine/gameobject/component/collision/CollisionLayer.h"

class CollisionManager;

/**
 * @brief コライダーの種類を表す列挙型
 */
enum class ColliderType
{
	AABB,	// 軸平行境界ボックス
	Sphere,	// 球
	OBB,	// 方向付き境界ボックス
	Ray,	// レイ（半直線）
};

/**
 * @brief 衝突判定コンポーネントの基底クラス
 * 
 * 全ての衝突判定コンポーネント（AABB、OBB、Sphere）の共通インターフェースです。
 * CollisionManagerに自動登録され、毎フレーム他のコライダーとの判定が行われます。
 * 
 * 主な機能:
 * - 衝突時のコールバック管理（OnEnter, OnStay, OnExit）
 * - サブステップ判定（高速移動体のすり抜け防止）
 * - 前フレーム位置の保存
 * - 判定サイズのオフセット調整
 * 
 * @note コンストラクタで自動的にCollisionManagerに登録されます
 * @note デストラクタで自動的にCollisionManagerから削除されます
 */
namespace GameObjectComponent
{
	class IGameObjectComponent;
	class IActionComponent;
	class ICollisionComponent;

	/**
	 * @brief 衝突判定の詳細情報を格納する構造体
	 */
	struct CollisionInfo
	{
		::GameObject* other = nullptr;                      // 衝突相手のGameObject
		ICollisionComponent* otherCollider = nullptr;      // 衝突相手のコライダー
		Vector3 normal = {};                                // 衝突法線（相手から自身へ向かう押し出し方向）
		float depth = 0.0f;                                 // めり込み深さ
		Vector3 collisionPoint = {};                        // 衝突位置
	};

	class ICollisionComponent : public virtual IGameObjectComponent
	{
	public:
		/**
		 * @brief デストラクタ
		 * 
		 * CollisionManagerから自動的に登録解除されます。
		 */
		virtual ~ICollisionComponent();
		
		/**
		 * @brief コンストラクタ
		 * 
		 * CollisionManagerに自動的に登録されます。
		 * 
		 * @param owner このコンポーネントを所有するGameObject
		 */
		ICollisionComponent(::GameObject* owner);

		/**
		 * @brief 前フレームの位置を設定
		 * 
		 * サブステップ判定で使用される前フレームの位置を記録します。
		 * フレームの最初にCollisionManagerから呼び出されます。
		 * 
		 * @param position 前フレームの位置
		 */
		void SetPreviousPosition(const Vector3& position) { previousPosition_ = position; }
		
		/**
		 * @brief 前フレームの位置を取得
		 * @return 前フレームの位置
		 */
		Vector3 GetPreviousPosition() const { return previousPosition_; }

		/**
		 * @brief サブステップ判定を使用するかを設定
		 * 
		 * 高速移動する弾丸などが薄い壁をすり抜けるのを防ぎます。
		 * 前フレーム位置から現在位置までを線分補間して判定します。
		 * 
		 * @param use true: サブステップ判定を使用, false: 通常判定
		 */
		void SetUseSubstep(bool use) { useSubstep_ = use; }
		
		/**
		 * @brief サブステップ判定を使用するかを取得
		 * @return サブステップ判定の使用状況
		 */
		bool UseSubstep() const { return useSubstep_; }

		/**
		 * @brief 衝突した位置を設定
		 * 
		 * サブステップ判定で衝突が発生した際の正確な位置を記録します。
		 * 
		 * @param position 衝突が発生した位置
		 */
		void SetCollisionPosition(const Vector3& position) { collisionPosition_ = position; }
		
		/**
		 * @brief 衝突した位置を取得
		 * @return 衝突が発生した位置
		 */
		Vector3 GetCollisionPosition() const { return collisionPosition_; }

		/**
		 * @brief 判定サイズのオフセットを設定
		 * 
		 * コライダーサイズの微調整に使用します。
		 * 
		 * @param offset サイズのオフセット値
		 */
		void SetSizeOffset(const Vector3& offset) { sizeOffset_ = offset; }
		
		/**
		 * @brief 判定サイズのオフセットを取得
		 * @return サイズのオフセット値
		 */
		Vector3 GetSizeOffset() const { return sizeOffset_; }

		/**
		 * @brief 衝突時のコールバック関数型
		 * 
		 * @param info 衝突の詳細情報
		 */
		using CollisionCallback = std::function<void(const CollisionInfo& info)>;

		/**
		 * @brief コライダーの種類を取得
		 * @return コライダーの種類（AABB, OBB, Sphere）
		 */
		virtual ColliderType GetColliderType() const = 0;

		/**
		 * @brief ブロードフェーズ（空間分割等）用のAABBを取得
		 * @return 境界AABB
		 */
		virtual ::AABB GetBroadphaseAABB() const = 0;

		/**
		 * @brief 衝突開始時のコールバックを設定
		 * @param callback 衝突開始時に呼び出される関数
		 */
		void SetOnEnter(CollisionCallback callback) { onEnter_ = callback; }
		
		/**
		 * @brief 衝突中のコールバックを設定
		 * @param callback 衝突している間毎フレーム呼び出される関数
		 */
		void SetOnStay(CollisionCallback callback) { onStay_ = callback; }
		
		/**
		 * @brief 衝突終了時のコールバックを設定
		 * @param callback 衝突が離れた時に呼び出される関数
		 */
		void SetOnExit(CollisionCallback callback) { onExit_ = callback; }

		/**
		 * @brief 自身が属する衝突レイヤーを設定
		 * @param layer 設定するレイヤー
		 */
		void SetCollisionLayer(ColliderLayer layer) { layer_ = layer; }

		/**
		 * @brief 自身が属する衝突レイヤーを取得
		 * @return 現在のレイヤー
		 */
		ColliderLayer GetCollisionLayer() const { return layer_; }

		/**
		 * @brief 衝突判定を行う相手のレイヤーマスクを設定
		 * @param mask 衝突判定を行うレイヤーマスク（論理和）
		 */
		void SetCollisionMask(uint32_t mask) { collisionMask_ = mask; }

		/**
		 * @brief 衝突判定を行う相手のレイヤーマスクを取得
		 * @return 現在のレイヤーマスク
		 */
		uint32_t GetCollisionMask() const { return collisionMask_; }

		/**
		 * @brief 衝突開始時のコールバックを実行
		 * @param info 衝突の詳細情報
		 */
		void CallOnEnter(const CollisionInfo& info) const { if (onEnter_) onEnter_(info); }
		
		/**
		 * @brief 衝突中のコールバックを実行
		 * @param info 衝突の詳細情報
		 */
		void CallOnStay(const CollisionInfo& info) const { if (onStay_) onStay_(info); }
		
		/**
		 * @brief 衝突終了時のコールバックを実行
		 * @param info 衝突の詳細情報
		 */
		void CallOnExit(const CollisionInfo& info) const { if (onExit_) onExit_(info); }

		/**
		 * @brief このコンポーネントを所有するGameObjectを取得
		 * @return 所有者のGameObject
		 */
		::GameObject* GetOwner() const { return owner_; }

		// --- 追加: アクティブ状態の制御 ---
		void SetActive(bool active) { isActive_ = active; }
		bool IsActive() const { return isActive_; }

		// --- 追加: 位置自動更新の制御 ---
		void SetAutoUpdatePosition(bool enable) { autoUpdatePosition_ = enable; }
		bool IsAutoUpdatePosition() const { return autoUpdatePosition_; }

	protected:
		// コンポーネントを所有するGameObject
		::GameObject* owner_ = nullptr;
		
		// 前フレームの位置（サブステップ判定用）
		Vector3 previousPosition_ = {};
		
		// 衝突した位置
		Vector3 collisionPosition_ = {};
		
		// サブステップ判定を行うか（高速移動体のすり抜け防止）
		bool useSubstep_ = false;
		
		// 判定サイズのオフセット（微調整用）
		Vector3 sizeOffset_ = {};

		// 自身が属する衝突レイヤー
		ColliderLayer layer_ = ColliderLayers::None;

		// 衝突判定を行う対象のレイヤーマスク
		uint32_t collisionMask_ = ColliderLayers::All;

		// --- 追加: 各種制御フラグ ---
		bool isActive_ = true;             // コライダーが有効かどうか
		bool autoUpdatePosition_ = true;   // GameObjectの位置に自動同期するか

	private:
		// 衝突開始時のコールバック
		CollisionCallback onEnter_ = nullptr;
		
		// 衝突中のコールバック
		CollisionCallback onStay_ = nullptr;
		
		// 衝突終了時のコールバック
		CollisionCallback onExit_ = nullptr;
	};
}
