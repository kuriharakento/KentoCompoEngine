#include "BarrierBlock.h"

void BarrierBlock::Initialize(Object3dCommon* object3dCommon, LightManager* lightManager)
{
	Obstacle::Initialize(object3dCommon, lightManager);
}

void BarrierBlock::Update()
{
	Obstacle::Update();
}

void BarrierBlock::Draw(CameraManager* camera)
{
	// バリアブロックは描画せずに行列の更新のみ行う
	UpdateTransform(camera);
}

void BarrierBlock::CollisionSettings(ICollisionComponent* collider)
{
	Obstacle::CollisionSettings(collider);
}
