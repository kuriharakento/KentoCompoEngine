#pragma once
#include "application/GameObject/component/base/ICollisionComponent.h"
#include "math/Sphere.h"

// 球コライダーコンポーネント
class SphereColliderComponent : public ICollisionComponent
{
public:
    SphereColliderComponent(GameObject* owner);

    // 球データ取得・設定
    const Sphere& GetSphere() const;
    void SetSphere(const Sphere& s);

    // オーナー座標に合わせて中心を更新
	void Update(GameObject* owner) override;

    // コライダータイプ
    ColliderType GetColliderType() const override;

private:
    Sphere sphere_;
};