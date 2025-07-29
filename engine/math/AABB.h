#pragma once
#include "Vector3.h"

struct AABB
{
	Vector3 min_;
	Vector3 max_;
	AABB() : min_(), max_() {}
	AABB(const Vector3& min, const Vector3& max) : min_(min), max_(max) {}

	Vector3 GetCenter() const
	{
		return (min_ + max_) * 0.5f;
	}
	Vector3 GetSize() const
	{
		return max_ - min_;
	}
	Vector3 GetHalfSize() const
	{
		return (max_ - min_) * 0.5f;
	}
};
