#pragma once

#include "Component.h"
#include "math/Vector3.h"
#include "math/AABB.h"
#include "engine/gameobject/component/collision/CollisionLayer.h"

namespace KCE
{
class GameObjectCollisionManager;
enum class ColliderType { AABB, Sphere, OBB, Ray };
namespace GameObjectComponent
{
class Collider;
/** @brief 衝突の相手と押し出し情報。 */
struct CollisionInfo
{
    Collider* self = nullptr;
    GameObject* other = nullptr;
    Collider* otherCollider = nullptr;
    Vector3 normal = {};
    float depth = 0.0f;
    Vector3 collisionPoint = {};
};
/** @brief 形状と衝突設定を持つコンポーネントの基底。 */
class Collider : public Component
{
public:
    ~Collider() override = default;
    /** @brief コライダーはワールド行列の確定後に更新する。 */
    bool IsCollider() const override { return true; }
    /** @brief 有効化時に衝突管理へ登録し、前位置を現在位置へ合わせる。 */
    void OnEnable() override;
    /** @brief 無効化時に衝突管理から登録解除する。 */
    void OnDisable() override;
    /** @brief 現在位置を前フレーム位置として記録する。 */
    void ResetPreviousPosition();
    /** @brief 前フレーム位置を設定する。 */
    void SetPreviousPosition(const Vector3& position) { previousPosition_ = position; }
    /** @brief 前フレーム位置を返す。 */
    Vector3 GetPreviousPosition() const { return previousPosition_; }
    /** @brief 連続判定を使うか設定する。 */
    void SetUseSubstep(bool use) { useSubstep_ = use; }
    /** @brief 連続判定を使うか返す。 */
    bool UseSubstep() const { return useSubstep_; }
    /** @brief 衝突位置を設定する。 */
    void SetCollisionPosition(const Vector3& position) { collisionPosition_ = position; }
    /** @brief 衝突位置を返す。 */
    Vector3 GetCollisionPosition() const { return collisionPosition_; }
    /** @brief 判定サイズの補正値を設定する。 */
    void SetSizeOffset(const Vector3& offset) { sizeOffset_ = offset; }
    /** @brief 判定サイズの補正値を返す。 */
    Vector3 GetSizeOffset() const { return sizeOffset_; }
    /** @brief 所有者中心からのローカル位置を設定する。 */
    void SetCenter(const Vector3& center) { centerOffset_ = center; }
    /** @brief 所有者中心からのローカル位置を返す。 */
    const Vector3& GetCenter() const { return centerOffset_; }
    /** @brief コライダー種別を返す。 */
    virtual ColliderType GetColliderType() const = 0;
    /** @brief ブロードフェーズ用の箱を返す。 */
    virtual AABB GetBroadphaseAABB() const = 0;
    /** @brief 自身の衝突レイヤーを設定する。 */
    void SetCollisionLayer(ColliderLayer layer) { layer_ = layer; }
    /** @brief 自身の衝突レイヤーを返す。 */
    ColliderLayer GetCollisionLayer() const { return layer_; }
    /** @brief 衝突対象のレイヤーマスクを設定する。 */
    void SetCollisionMask(uint32_t mask) { collisionMask_ = mask; }
    /** @brief 衝突対象のレイヤーマスクを返す。 */
    uint32_t GetCollisionMask() const { return collisionMask_; }
    /** @brief GameObject の位置へ自動追従するか設定する。 */
    void SetAutoUpdatePosition(bool enable) { autoUpdatePosition_ = enable; }
    /** @brief GameObject の位置へ自動追従するか返す。 */
    bool IsAutoUpdatePosition() const { return autoUpdatePosition_; }
    /** @brief GameObject も含めて判定が有効か返す。 */
    bool IsActive() const;
    /** @brief 組単位の Enter を所有者へ送る。 */
    void CallOnEnter(const CollisionInfo& info) const;
    /** @brief 組単位の Stay を所有者へ送る。 */
    void CallOnStay(const CollisionInfo& info) const;
    /** @brief 組単位の Exit を所有者へ送る。 */
    void CallOnExit(const CollisionInfo& info) const;
protected:
    Vector3 previousPosition_ = {};
    Vector3 collisionPosition_ = {};
    bool useSubstep_ = false;
    Vector3 sizeOffset_ = {};
    Vector3 centerOffset_ = {};
    ColliderLayer layer_ = ColliderLayers::None;
    uint32_t collisionMask_ = ColliderLayers::All;
    bool autoUpdatePosition_ = true;
};
}
} // namespace KCE
