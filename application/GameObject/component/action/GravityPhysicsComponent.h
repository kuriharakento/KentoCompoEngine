#pragma once

#include "application/GameObject/component/base/IActionComponent.h"
#include "math/Vector3.h"

/// 重力演算専用コンポーネント（Physics 系）
class GravityPhysicsComponent : public IActionComponent
{
public:
    /// @param gravity 重力加速度（デフォルト 9.8f）
    explicit GravityPhysicsComponent(float gravity = 9.8f);

    /// フレームごとに垂直速度を更新し、Y=0 より下には落とさない
    void Update(GameObject* owner) override;

    /// 描画は不要
    void Draw(CameraManager* camera) override {}

private:
    float gravity_;            // 重力加速度
    float verticalVelocity_;   // 現在の垂直速度
};