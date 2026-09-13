#pragma once

namespace KCE
{
class CameraManager;
class GameObject;
namespace GameObjectComponent
{
struct CollisionInfo;
/** @brief GameObject が所有する全コンポーネントの基底クラス。 */
class Component
{
public:
    virtual ~Component() = default;
    /** @brief 追加直後に一度だけ呼ばれる。 */
    virtual void Awake() {}
    /** @brief 有効になったときに呼ばれる。 */
    virtual void OnEnable() {}
    /** @brief 最初の更新直前に一度だけ呼ばれる。 */
    virtual void Start() {}
    /** @brief 毎フレームの更新を行う。 */
    virtual void Update() {}
    /** @brief 全コンポーネントの Update 後に呼ばれる。 */
    virtual void LateUpdate() {}
    /** @brief 無効になる直前に呼ばれる。 */
    virtual void OnDisable() {}
    /** @brief 破棄直前に一度だけ呼ばれる。 */
    virtual void OnDestroy() {}
    /** @brief コライダーの組が触れ始めたときに呼ばれる。 */
    virtual void OnCollisionEnter(const CollisionInfo&) {}
    /** @brief コライダーの組が触れている間に呼ばれる。 */
    virtual void OnCollisionStay(const CollisionInfo&) {}
    /** @brief コライダーの組が離れたときに呼ばれる。 */
    virtual void OnCollisionExit(const CollisionInfo&) {}
    /** @brief GameObject の組が触れ始めたときに一度呼ばれる。 */
    virtual void OnObjectCollisionEnter(const CollisionInfo&) {}
    /** @brief GameObject の組が触れている間に一度呼ばれる。 */
    virtual void OnObjectCollisionStay(const CollisionInfo&) {}
    /** @brief GameObject の組が離れたときに一度呼ばれる。 */
    virtual void OnObjectCollisionExit(const CollisionInfo&) {}
    /** @brief コンポーネント単体の有効状態を変える。 */
    void SetEnabled(bool enabled);
    /** @brief コンポーネント単体の有効状態を返す。 */
    bool IsEnabled() const { return enabled_; }
    /** @brief 所有する GameObject を返す。所有権は移らない。 */
    GameObject* GetOwner() const { return owner_; }
private:
    friend class KCE::GameObject;
    // owner_ は GameObject が所有し、Component より長く生きる。
    GameObject* owner_ = nullptr;
    bool enabled_ = true;
    bool awakeCalled_ = false;
    bool started_ = false;
    bool activeInHierarchy_ = false;
    bool destroyed_ = false;
};
}
} // namespace KCE
