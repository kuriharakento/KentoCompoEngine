#include "OrbitCameraWork.h"

// math
#include "math/MathUtils.h"
#include <time/TimeManager.h>

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
    
    // カメラの回転を計算して設定
	Vector3 rotation = MathUtils::CalculateYawPitchFromDirection(toTarget);
    camera_->SetRotate(rotation);
    
	// 時間経過（デルタタイムタイプに応じて選択）
	float deltaTime = (deltaType_ == DeltaTimeType::DeltaTime) ?
		TimeManager::GetInstance().GetGameContext().deltaTime :
		TimeManager::GetInstance().GetGameContext().realDeltaTime;
	time_ += speed_ * deltaTime;
}

void OrbitCameraWork::Start(Vector3 target, float radius, float speed, float initialAngle, DeltaTimeType deltaType)
{
    // パラメータを設定
	targetValue_ = target;
	radius_ = radius;
	speed_ = speed;
    
    // 初期角度を正規化して設定
	time_ = MathUtils::NormalizeAngleRad(initialAngle);
	isActive_ = true;
	deltaType_ = deltaType;
}

void OrbitCameraWork::Start(const Vector3* target, float radius, float speed, float initialAngle, DeltaTimeType deltaType)
{
    // ポインタでターゲットを設定（動的追従用）
	targetPtr_ = target;
	radius_ = radius;
	speed_ = speed;
    
    // 初期角度を正規化して設定
	time_ = MathUtils::NormalizeAngleRad(initialAngle);
	isActive_ = true;
	deltaType_ = deltaType;
}