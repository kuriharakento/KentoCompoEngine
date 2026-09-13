#include "Collider.h"
#include "engine/gameobject/base/GameObject.h"
#include "engine/gameobject/component/collision/CollisionManager.h"
#include "math/MathUtils.h"

namespace KCE::GameObjectComponent
{
void Collider::OnEnable()
{
    ResetPreviousPosition();
    CollisionManager::GetInstance()->Register(this);
}
void Collider::OnDisable()
{
    CollisionManager::GetInstance()->Unregister(this);
}
void Collider::ResetPreviousPosition()
{
    if (GetOwner())
    {
        previousPosition_ = MathUtils::GetTranslateFromMatrix(GetOwner()->GetWorldMatrix());
    }
}
bool Collider::IsActive() const
{
    return IsEnabled() && GetOwner() && GetOwner()->IsActive();
}
void Collider::CallOnEnter(const CollisionInfo& info) const
{
    if (GetOwner()) GetOwner()->DispatchCollisionEnter(info);
}
void Collider::CallOnStay(const CollisionInfo& info) const
{
    if (GetOwner()) GetOwner()->DispatchCollisionStay(info);
}
void Collider::CallOnExit(const CollisionInfo& info) const
{
    if (GetOwner()) GetOwner()->DispatchCollisionExit(info);
}
} // namespace KCE::GameObjectComponent
