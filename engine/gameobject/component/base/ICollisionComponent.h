#pragma once
#include <functional>

#include "IGameObjectComponent.h"
#include "math/Vector3.h"

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
		 * @param other 衝突相手のGameObject
		 */
		using CollisionCallback = std::function<void(::GameObject* other)>;

		/**
		 * @brief コライダーの種類を取得
		 * @return コライダーの種類（AABB, OBB, Sphere）
		 */
		virtual ColliderType GetColliderType() const = 0;

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
		 * @brief 衝突開始時のコールバックを実行
		 * @param other 衝突相手のGameObject
		 */
		void CallOnEnter(::GameObject* other) const { if (onEnter_) onEnter_(other); }
		
		/**
		 * @brief 衝突中のコールバックを実行
		 * @param other 衝突相手のGameObject
		 */
		void CallOnStay(::GameObject* other) const { if (onStay_) onStay_(other); }
		
		/**
		 * @brief 衝突終了時のコールバックを実行
		 * @param other 衝突相手のGameObject
		 */
		void CallOnExit(::GameObject* other) const { if (onExit_) onExit_(other); }

		/**
		 * @brief このコンポーネントを所有するGameObjectを取得
		 * @return 所有者のGameObject
		 */
		::GameObject* GetOwner() const { return owner_; }

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

	private:
		// 衝突開始時のコールバック
		CollisionCallback onEnter_ = nullptr;
		
		// 衝突中のコールバック
		CollisionCallback onStay_ = nullptr;
		
		// 衝突終了時のコールバック
		CollisionCallback onExit_ = nullptr;
	};
}