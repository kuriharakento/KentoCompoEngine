#include "Component.h"
#include "engine/gameobject/base/GameObject.h"

namespace KCE::GameObjectComponent
{
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
