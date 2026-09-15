#include "Component.h"
#include "engine/gameobject/base/GameObject.h"
#include "time/TimeManager.h"

namespace KCE::GameObjectComponent
{
float Component::GetDeltaTime() const
{
    // 持ち主が決まる前（Awake より前）は Game の時計で答える
    return owner_ ? owner_->GetDeltaTime() : TimeManager::GetInstance().GetGameContext().deltaTime;
}

void Component::SetEnabled(bool enabled)
{
    if (enabled_ == enabled)
    {
        return;
    }
    enabled_ = enabled;
    if (owner_)
    {
        owner_->RefreshComponentActivation();
    }
}
} // namespace KCE::GameObjectComponent
