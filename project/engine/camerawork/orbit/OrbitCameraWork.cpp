#include "OrbitCameraWork.h"

// math
#include "math/MathUtils.h"

void OrbitCameraWork::Initialize(Camera* camera)
{
	camera_ = camera;
	time_ = 0.0f;
	isActive_ = false;
}

void OrbitCameraWork::Update()
{
    if (!isActive_) return;

	Vector3 targetPosition = GetTarget();
    // カメラ位置を円軌道で計算
	Vector3 cameraPosition = MathUtils::CalculateOrbitPosition(targetPosition, radius_, time_);
    camera_->SetTranslate(cameraPosition + positionOffset);
    // ターゲット方向のベクトルを取得
    Vector3 toTarget = targetPosition - cameraPosition;
    //カメラの回転を計算して設定
	Vector3 rotation = MathUtils::CalculateYawPitchFromDirection(toTarget);
    camera_->SetRotate(rotation);
	//時間経過
    time_ += speed_ * 0.016f;
}

void OrbitCameraWork::Start(Vector3 target, float radius, float speed)
{
	targetValue_ = target;
	radius_ = radius;
	speed_ = speed;
	isActive_ = true;
}

void OrbitCameraWork::Start(const Vector3* target, float radius, float speed)
{
	targetPtr_ = target;
	radius_ = radius;
	speed_ = speed;
	isActive_ = true;
}
