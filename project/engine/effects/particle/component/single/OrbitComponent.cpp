#include "OrbitComponent.h"

OrbitComponent::OrbitComponent(const Vector3& c, float radius_, float speed)
    : center_(c), radius_(radius_), angularSpeed_(speed)
{
}

OrbitComponent::OrbitComponent(const Vector3* target, float radius_, float speed) : target_(target), radius_(radius_), angularSpeed_(speed)
{
	// 動的中心が指定されている場合は初期値を設定
	if (target_)
	{
		center_ = *target_;
	}
	else
	{
		// ポインタがnullの場合はゼロベクトルを使用
        center_ = Vector3();
	}
}

void OrbitComponent::Update(Particle& particle)
{
	// 動的中心が設定されている場合は毎フレーム座標を更新
	if (target_)
	{
		center_ = *target_;
	}

	// 今フレームの回転角度を取得
    float angle = angularSpeed_;

	// 中心からのオフセットベクトルを計算
    Vector3 offset = particle.transform.translate - center_;

	// Y軸周りの回転行列の要素を計算
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);

	// XZ平面上で回転を適用（Y軸周りの2D回転）
    float x = offset.x * cosA - offset.z * sinA;
    float z = offset.x * sinA + offset.z * cosA;

	// 回転後のオフセットを設定
    offset.x = x;
    offset.z = z;

	// 新しい位置を計算（中心 + 回転後のオフセット）
    particle.transform.translate = center_ + offset;
}
