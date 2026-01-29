#pragma once
#include "AABBColliderComponent.h"
#include "OBBColliderComponent.h"
#include "SphereColliderComponent.h"

/**
 * @brief 2D判定時の衝突判定面を表す列挙型
 */
enum class CollisionPlane
{
	XY, // XY平面（横スクロール、トップビュー）
	XZ, // XZ平面（3D世界の地面）
	YZ, // YZ平面（縦スクロール）
};

/**
 * @brief 衝突判定アルゴリズムを提供する名前空間
 * 
 * 各種コライダーの組み合わせに対する衝突判定関数を提供します。
 * 2D/3Dモード、通常判定/サブステップ判定に対応しています。
 * 
 * サポートする判定:
 * - AABB vs AABB
 * - OBB vs OBB（分離軸定理）
 * - AABB vs OBB
 * - Sphere vs Sphere
 * - Sphere vs AABB
 * - Sphere vs OBB
 * 
 * @note サブステップ判定は高速移動体のすり抜けを防ぎます
 */
namespace collisionAlgorithm
{
	// --- 3D用判定 ---
	
	/**
	 * @brief AABB同士の3D衝突判定
	 * @param a 判定対象のAABBコライダーA
	 * @param b 判定対象のAABBコライダーB
	 * @return 衝突している場合true
	 */
	bool CheckAABBvsAABB3D(const AABBColliderComponent* a, const AABBColliderComponent* b);
	
	/**
	 * @brief OBB同士の3D衝突判定
	 * 
	 * 分離軸定理（SAT）を使用して判定します。
	 * 
	 * @param a 判定対象のOBBコライダーA
	 * @param b 判定対象のOBBコライダーB
	 * @return 衝突している場合true
	 */
	bool CheckOBBvsOBB3D(const OBBColliderComponent* a, const OBBColliderComponent* b);
	
	/**
	 * @brief AABBとOBBの3D衝突判定
	 * @param a 判定対象のAABBコライダー
	 * @param b 判定対象のOBBコライダー
	 * @return 衝突している場合true
	 */
	bool CheckAABBvsOBB3D(const AABBColliderComponent* a, const OBBColliderComponent* b);
	
	/**
	 * @brief Sphere同士の3D衝突判定
	 * @param a 判定対象のSphereコライダーA
	 * @param b 判定対象のSphereコライダーB
	 * @return 衝突している場合true
	 */
	bool CheckSpherevsSphere3D(const SphereColliderComponent* a, const SphereColliderComponent* b);
	
	/**
	 * @brief SphereとAABBの3D衝突判定
	 * @param a 判定対象のSphereコライダー
	 * @param b 判定対象のAABBコライダー
	 * @return 衝突している場合true
	 */
	bool CheckSpherevsAABB3D(const SphereColliderComponent* a, const AABBColliderComponent* b);
	
	/**
	 * @brief SphereとOBBの3D衝突判定
	 * @param a 判定対象のSphereコライダー
	 * @param b 判定対象のOBBコライダー
	 * @return 衝突している場合true
	 */
	bool CheckSpherevsOBB3D(const SphereColliderComponent* a, const OBBColliderComponent* b);

	// サブステップ判定（3D）
	
	/**
	 * @brief AABB同士のサブステップ3D衝突判定
	 * 
	 * 前フレーム位置から現在位置までを線分補間して判定します。
	 * 高速移動する弾丸などのすり抜けを防止します。
	 * 
	 * @param a 判定対象のAABBコライダーA
	 * @param b 判定対象のAABBコライダーB
	 * @return 衝突している場合true
	 */
	bool CheckAABBvsAABBSubstep3D(const AABBColliderComponent* a, const AABBColliderComponent* b);
	
	/**
	 * @brief OBB同士のサブステップ3D衝突判定
	 * @param a 判定対象のOBBコライダーA
	 * @param b 判定対象のOBBコライダーB
	 * @return 衝突している場合true
	 */
	bool CheckOBBvsOBBSubstep3D(const OBBColliderComponent* a, const OBBColliderComponent* b);
	
	/**
	 * @brief AABBとOBBのサブステップ3D衝突判定
	 * @param a 判定対象のAABBコライダー
	 * @param b 判定対象のOBBコライダー
	 * @return 衝突している場合true
	 */
	bool CheckAABBvsOBBSubstep3D(const AABBColliderComponent* a, const OBBColliderComponent* b);
	
	/**
	 * @brief Sphere同士のサブステップ3D衝突判定
	 * @param a 判定対象のSphereコライダーA
	 * @param b 判定対象のSphereコライダーB
	 * @return 衝突している場合true
	 */
	bool CheckSpherevsSphereSubstep3D(const SphereColliderComponent* a, const SphereColliderComponent* b);
	
	/**
	 * @brief SphereとAABBのサブステップ3D衝突判定
	 * @param a 判定対象のSphereコライダー
	 * @param b 判定対象のAABBコライダー
	 * @return 衝突している場合true
	 */
	bool CheckSpherevsAABBSubstep3D(const SphereColliderComponent* a, const AABBColliderComponent* b);
	
	/**
	 * @brief SphereとOBBのサブステップ3D衝突判定
	 * @param a 判定対象のSphereコライダー
	 * @param b 判定対象のOBBコライダー
	 * @return 衝突している場合true
	 */
	bool CheckSpherevsOBBSubstep3D(const SphereColliderComponent* a, const OBBColliderComponent* b);


	// --- 2D用判定（平面指定） ---
	
	/**
	 * @brief AABB同士の2D衝突判定
	 * @param a 判定対象のAABBコライダーA
	 * @param b 判定対象のAABBコライダーB
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckAABBvsAABB2D(const AABBColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief OBB同士の2D衝突判定
	 * @param a 判定対象のOBBコライダーA
	 * @param b 判定対象のOBBコライダーB
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckOBBvsOBB2D(const OBBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane);
	
	bool CheckOBBvsOBB_XY(const OBB& obbA, const OBB& obbB);
	bool CheckOBBvsOBB_XZ(const OBB& obbA, const OBB& obbB);
	bool CheckOBBvsOBB_YZ(const OBB& obbA, const OBB& obbB);
	
	/**
	 * @brief AABBとOBBの2D衝突判定
	 * @param a 判定対象のAABBコライダー
	 * @param b 判定対象のOBBコライダー
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckAABBvsOBB2D(const AABBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief Circle同士の2D衝突判定
	 * 
	 * 2DモードではSphereが円として扱われます。
	 * 
	 * @param a 判定対象のSphereコライダーA
	 * @param b 判定対象のSphereコライダーB
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckCirclevsCircle2D(const SphereColliderComponent* a, const SphereColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief CircleとAABBの2D衝突判定
	 * @param a 判定対象のSphereコライダー
	 * @param b 判定対象のAABBコライダー
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckCirclevsAABB2D(const SphereColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief CircleとOBBの2D衝突判定
	 * @param a 判定対象のSphereコライダー
	 * @param b 判定対象のOBBコライダー
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckCirclevsOBB2D(const SphereColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane);

	// サブステップ判定（2D）
	
	/**
	 * @brief AABB同士のサブステップ2D衝突判定
	 * @param a 判定対象のAABBコライダーA
	 * @param b 判定対象のAABBコライダーB
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckAABBvsAABBSubstep2D(const AABBColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief OBB同士のサブステップ2D衝突判定
	 * @param a 判定対象のOBBコライダーA
	 * @param b 判定対象のOBBコライダーB
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckOBBvsOBBSubstep2D(const OBBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief AABBとOBBのサブステップ2D衝突判定
	 * @param a 判定対象のAABBコライダー
	 * @param b 判定対象のOBBコライダー
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckAABBvsOBBSubstep2D(const AABBColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief Circle同士のサブステップ2D衝突判定
	 * @param a 判定対象のSphereコライダーA
	 * @param b 判定対象のSphereコライダーB
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckCirclevsCircleSubstep2D(const SphereColliderComponent* a, const SphereColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief CircleとAABBのサブステップ2D衝突判定
	 * @param a 判定対象のSphereコライダー
	 * @param b 判定対象のAABBコライダー
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckCirclevsAABBSubstep2D(const SphereColliderComponent* a, const AABBColliderComponent* b, CollisionPlane plane);
	
	/**
	 * @brief CircleとOBBのサブステップ2D衝突判定
	 * @param a 判定対象のSphereコライダー
	 * @param b 判定対象のOBBコライダー
	 * @param plane 判定する平面（XY, XZ, YZ）
	 * @return 衝突している場合true
	 */
	bool CheckCirclevsOBBSubstep2D(const SphereColliderComponent* a, const OBBColliderComponent* b, CollisionPlane plane);
}

